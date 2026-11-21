const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const Vue = require('vue');

function load(file, context) {
    const source = fs.readFileSync(path.join(__dirname, '..', file), 'utf8')
        .split('<script>')[1].split('</script>')[0]
        .replace(/^import .*;$/gm, '').replace('export default', 'this.component =');
    vm.runInNewContext(source, context);
    return context.component;
}

(async () => {
    const observers = [];
    const context = {
        window: { innerHeight: 800 },
        IntersectionObserver: class {
            constructor(callback) { this.callback = callback; this.targets = new Set(); observers.push(this); }
            observe(target) { this.targets.add(target); }
            unobserve(target) { this.targets.delete(target); }
            disconnect() { this.targets.clear(); }
        },
    };
    const component = load('src/components/GalleryViewportItem.vue', context);
    const items = Array.from({ length: 500 }, () => {
        const item = new Vue(component);
        item.$el = { getBoundingClientRect: () => ({ height: 374, top: 5000, bottom: 5374 }) };
        component.mounted.call(item);
        return item;
    });
    assert.equal(observers.length, 1, 'Placeholders must share one observer');
    assert(items.every(item => !item.active), 'Do not mount offscreen contents initially');
    // Scroll through a large gallery and back: mounted content must not accumulate.
    for (const start of [0, 50, 100, 300, 480, 250, 0]) {
        observers[0].callback(items.map((item, index) => ({
            target: item.$el, isIntersecting: index >= start && index < start + 20,
        })));
        assert.equal(items.filter(item => item.active).length, 20);
    }
    assert.equal(items[100].placeholderHeight, 374, 'Retain measured height when unloading');
    items[0].pinned = true;
    await Vue.nextTick();
    observers[0].callback([{ target: items[0].$el, isIntersecting: false }]);
    assert(items[0].active, 'Keep an open information dialog mounted');
    items[0].pinned = false;
    await Vue.nextTick();
    assert(!items[0].active, 'Release a pinned card once its dialog closes');
    const immediate = new Vue(component);
    immediate.$el = { getBoundingClientRect: () => ({ top: 200, bottom: 574, height: 374 }) };
    component.mounted.call(immediate);
    assert(immediate.active, 'Mount new nearby results before the first observer callback');
    immediate.$destroy();
    items.forEach(item => item.$destroy());
    assert.equal(observers[0].targets.size, 0, 'Release observer targets on navigation/search');

    const fallbackComponent = load('src/components/GalleryViewportItem.vue', {});
    const fallback = new Vue(fallbackComponent);
    fallbackComponent.mounted.call(fallback);
    assert(fallback.active, 'Keep gallery usable without IntersectionObserver');
    fallback.$destroy();

    let cleared = 0, disconnected = 0, removed = 0;
    const canvas = load('src/components/BmpCanvas.vue', {
        clearInterval: () => cleared++,
        document: { removeEventListener: () => removed++ },
    });
    const bitmap = {
        animationTimer: 42, frames: [[1], [2]],
        preloadObserver: { disconnect: () => disconnected++ },
        visibilityObserver: { disconnect: () => disconnected++ },
    };
    bitmap.stopAnimation = canvas.methods.stopAnimation.bind(bitmap);
    canvas.beforeDestroy.call(bitmap);
    assert.equal(cleared, 1);
    assert.equal(disconnected, 2);
    assert.equal(removed, 1);
    assert.equal(bitmap.frames, null, 'Release parsed frames when a card unmounts');
    // Render the actual gallery template: filtering used to produce a negative
    // v-for count for placeholders, aborting Vue's render for fewer than 24 hits.
    const scrollWindow = {
        scrollY: 0,
        scrollTo(options) { this.scrollY = options.top; this.lastScroll = options; },
    };
    const stored = new Map();
    const localStorage = {
        getItem: key => stored.get(key) || null,
        setItem: (key, value) => stored.set(key, value),
    };
    const uuid = 'a'.repeat(40);
    const storedVotes = [
        { iconId: 3, average: 3, count: 2, myRating: null },
        { iconId: 5, average: 5, count: 1, myRating: null },
    ];
    const ratingRequests = [];
    let ratingsFailure = null;
    const ratingFetch = async (url, options) => {
        assert.equal(url, 'https://example.test/api/icon-ratings');
        const voter = options.headers['X-PixelIt-Rating-UUID'];
        assert(voter === undefined || voter === uuid || voter === 'b'.repeat(40));
        ratingRequests.push(options);
        if (ratingsFailure === 'network') throw new TypeError('Failed to fetch');
        if (ratingsFailure) return { ok: false, status: ratingsFailure };
        if (options.method === 'POST') {
            assert.equal(options.headers['Content-Type'], 'application/json');
            const { iconId, stars } = JSON.parse(options.body);
            assert.equal(iconId, 10);
            const row = { iconId, average: stars === 1 ? 4.5 : 4.9, count: 10, myRating: stars };
            const index = storedVotes.findIndex(vote => vote.iconId === iconId);
            if (index === -1) storedVotes.push(row);
            else storedVotes.splice(index, 1, row);
            return { ok: true, json: async () => row };
        }
        return { ok: true, json: async () => storedVotes.map(row => ({ ...row, myRating: voter === uuid ? row.myRating : null })) };
    };
    const galleryComponent = load('src/views/Gallery.vue', {
        window: scrollWindow, localStorage, fetch: ratingFetch,
        ...require('./gallery-helpers.cjs'), actionFeedback: {}, notify() {}, BmpCanvas: {}, BmpDialog: {}, ButtonBMPReport: {}, GalleryViewportItem: {},
    });
    const gallerySource = fs.readFileSync(path.join(__dirname, '../src/views/Gallery.vue'), 'utf8');
    const compiled = require('vue-template-compiler').compileToFunctions(
        gallerySource.slice('<template>'.length, gallerySource.lastIndexOf('</template>'))
    );
    const records = Array.from({ length: 101 }, (_, id) => ({ id, name: `Bitmap ${id}` }));
    const gallery = new Vue({
        ...galleryComponent, ...compiled,
        beforeCreate() {
            this.$apiServerBaseURL = 'https://example.test/api/';
            this.$vuetify = { application: { top: 64 } };
            this.$store = Vue.observable({ state: { bmpsFromAPI: records, deviceUuid: uuid, socket: { isConnected: true } } });
        },
    });
    gallery.$el = { getBoundingClientRect: () => ({ top: 160 - scrollWindow.scrollY }) };
    gallery.automaticLoading = true;
    const renderErrors = [];
    const previousErrorHandler = Vue.config.errorHandler;
    Vue.config.errorHandler = error => renderErrors.push(error);
    try {
        for (const count of [0, 1, 23, 24, 25]) {
            gallery.$store.state.bmpsFromAPI.splice(0, records.length,
                ...Array.from({ length: count }, (_, id) => ({ id, name: 'Match' })));
            gallery.message = 'Match';
            await Vue.nextTick();
            gallery._render();
            assert.equal(gallery.getBMPs.length, count);
            assert.equal(gallery.placeholderCount, count > 24 ? count - 24 : 0);
            assert.equal(gallery.hasMoreBMPs, count > 24);
        }
        records.splice(0, records.length,
            ...Array.from({ length: 101 }, (_, id) => ({ id, name: `Bitmap ${id}` })));
        gallery.message = ''; await Vue.nextTick();
        gallery.visibleCount = 96;
        scrollWindow.scrollY = 7000;
        gallery.message = '100'; await Vue.nextTick();
        gallery._render();
        assert.equal(gallery.visibleCount, 24, 'Search resets the batch after scrolling');
        assert.equal(scrollWindow.scrollY, 88, 'Search resets deep scroll position before replacing results');
        assert.equal(gallery.visibleBMPs.length, 1);
        assert.equal(gallery.visibleBMPs[0].id, 100, 'Search includes entries not yet displayed');
        gallery.message = 'no results'; await Vue.nextTick(); gallery._render();
        assert.equal(gallery.visibleBMPs.length, 0);
        gallery.message = ''; await Vue.nextTick(); gallery._render();
        assert.equal(gallery.visibleBMPs.length, 24, 'Clearing search restores the initial batch');
        assert(gallery.hasMoreBMPs);
        // Animation filtering must combine with name/ID search across the complete cache.
        records.splice(0, records.length, ...Array.from({ length: 80 }, (_, id) => ({
            id, name: id >= 60 ? 'Rocket' : 'Other', animated: id % 2 === 0,
            sizeX: String([8, 16, 32][id % 3]), sizeY: 8,
        })));
        gallery.visibleCount = 72;
        scrollWindow.scrollY = 8000;
        const revisionBeforeFilter = gallery.filterRevision;
        gallery.animationFilter = 'animated'; await Vue.nextTick(); gallery._render();
        assert.equal(gallery.visibleCount, 24, 'Changing the animation filter resets the batch');
        assert.equal(scrollWindow.scrollY, 88, 'Reset deep scroll position to results below the app bar');
        assert.equal(scrollWindow.lastScroll.behavior, 'instant', 'Do not animate through empty result space');
        assert(gallery.filterRevision > revisionBeforeFilter, 'Recreate retained cards after filtering');
        assert.equal(gallery.getBMPs.length, 40);
        assert(gallery.getBMPs.every(bmp => bmp.animated));
        assert(gallery.hasMoreBMPs);
        gallery.showMoreBMPs();
        assert.equal(gallery.visibleBMPs.length, 40, 'Scrolling stays within the selected filter');
        gallery.message = 'rOcKeT'; await Vue.nextTick(); gallery._render();
        assert.equal(gallery.getBMPs.length, 10);
        assert(gallery.getBMPs.every(bmp => bmp.id >= 60 && bmp.animated));
        gallery.animationFilter = 'static'; await Vue.nextTick(); gallery._render();
        assert.equal(gallery.getBMPs.length, 10);
        assert(gallery.getBMPs.every(bmp => bmp.id >= 60 && !bmp.animated));
        gallery.message = '78'; await Vue.nextTick(); gallery._render();
        assert.equal(gallery.getBMPs.length, 0, 'An exact ID must also respect the animation filter');
        gallery.animationFilter = 'animated'; await Vue.nextTick(); gallery._render();
        assert.equal(gallery.visibleBMPs[0].id, 78);
        gallery.animationFilter = 'all'; await Vue.nextTick();
        assert.equal(gallery.getBMPs.length, 1, 'Changing the filter preserves the search text');
        gallery.message = ''; await Vue.nextTick(); gallery._render();
        assert.equal(gallery.getBMPs.length, 80, 'All bitmaps restores animated and static records');
        records.push({ id: 80, name: 'Tall', animated: false, sizeX: 8, sizeY: '16' });
        await Vue.nextTick();
        assert.deepEqual(Array.from(gallery.sizeFilters, option => option.value),
            ['all', '8x8', '8x16', '16x8', '32x8'], 'Size options are unique, sorted and include both dimensions');
        gallery.visibleCount = 72;
        scrollWindow.scrollY = 6000;
        gallery.sizeFilter = '8x8'; await Vue.nextTick(); gallery._render();
        assert.equal(scrollWindow.scrollY, 88, 'Size changes also reset deep scroll position');
        assert.equal(gallery.visibleCount, 24, 'Changing the size filter resets the batch');
        assert.equal(gallery.getBMPs.length, 27);
        assert(gallery.hasMoreBMPs);
        gallery.showMoreBMPs(); assert.equal(gallery.visibleBMPs.length, 27);
        gallery.animationFilter = 'animated'; await Vue.nextTick(); gallery._render();
        assert.equal(gallery.getBMPs.length, 14);
        gallery.message = 'rocket'; await Vue.nextTick(); gallery._render();
        assert.equal(gallery.getBMPs.length, 4, 'Size, animation and name search combine');
        assert.equal(gallery.sizeFilters.length, 5, 'Search must not remove size options');
        gallery.message = '78'; await Vue.nextTick();
        assert.equal(gallery.visibleBMPs[0].id, 78);
        gallery.sizeFilter = '32x8'; await Vue.nextTick(); gallery._render();
        assert.equal(gallery.getBMPs.length, 0, 'Exact ID must respect the selected size');
        assert.equal(gallery.message, '78');
        assert.equal(gallery.animationFilter, 'animated');
        gallery.message = ''; gallery.animationFilter = 'static'; gallery.sizeFilter = '8x16';
        await Vue.nextTick(); gallery._render();
        assert.equal(gallery.visibleBMPs.length, 1);
        assert.equal(gallery.visibleBMPs[0].id, 80, 'Same width with a different height is a distinct size');
        gallery.sizeFilter = 'all'; await Vue.nextTick();
        assert.equal(gallery.getBMPs.length, 41, 'All sizes preserves the animation filter');
        gallery.animationFilter = 'all'; await Vue.nextTick(); gallery._render();
        assert.equal(gallery.getBMPs.length, 81);
        gallery.visibleCount = 72;
        scrollWindow.scrollY = 6000;
        gallery.sortOrder = 'id-desc'; await Vue.nextTick(); gallery._render();
        assert.equal(gallery.visibleCount, 24);
        assert.equal(scrollWindow.scrollY, 88);
        assert.equal(gallery.visibleBMPs[0].id, 80, 'Sort before selecting the visible batch');
        gallery.sortOrder = 'id-asc'; await Vue.nextTick();
        assert.equal(gallery.visibleBMPs[0].id, 0);

        records.splice(0, records.length,
            { id: 10, name: 'Rocket 10', animated: true, sizeX: 8, sizeY: 8 },
            { id: 3, name: 'Rocket 2', animated: true, sizeX: 8, sizeY: 8 },
            { id: 5, name: 'Rocket 1', animated: false, sizeX: 8, sizeY: 8 },
            { id: 9, name: 'Rocket', animated: true, sizeX: 32, sizeY: 8 },
            { id: 7, name: 'Other', animated: true, sizeX: 8, sizeY: 8 });
        gallery.message = 'rocket';
        gallery.animationFilter = 'animated';
        gallery.sizeFilter = '8x8';
        gallery.sortOrder = 'name-asc'; await Vue.nextTick(); gallery._render();
        assert.deepEqual(Array.from(gallery.getBMPs, bmp => bmp.id), [3, 10]);
        gallery.sortOrder = 'name-desc'; await Vue.nextTick();
        assert.deepEqual(Array.from(gallery.getBMPs, bmp => bmp.id), [10, 3]);
        assert.deepEqual(records.map(bmp => bmp.id), [10, 3, 5, 9, 7], 'Sorting preserves the cache order');
        gallery.sortOrder = 'default'; await Vue.nextTick();
        assert.deepEqual(Array.from(gallery.getBMPs, bmp => bmp.id), [10, 3]);
        gallery.sortOrder = 'rating-desc'; await Vue.nextTick(); gallery._render();
        assert.deepEqual(Array.from(gallery.getBMPs, bmp => bmp.id), [3, 10], 'Sort matching icons by community rating');
        assert.equal(gallery.ratingUuid, uuid);
        await Promise.all([gallery.rateBitmap(10, 1), gallery.rateBitmap(10, 5)]);
        await gallery.rateBitmap(10, 1);
        assert.equal(ratingRequests.filter(options => options.method === 'POST').length, 1);
        assert.equal(gallery.myRating(10), 1);
        assert.equal(gallery.ratings[10].average, 4.5);
        assert.equal(gallery.ratingVoteLabel(10), '10 votes');
        assert.equal(gallery.ratingVoteLabel(5), '1 vote');
        assert.deepEqual(Array.from(gallery.getBMPs, bmp => bmp.id), [10, 3], 'Sort by the average returned by the API, not the personal vote');
        await gallery.rateBitmap(10, 5);
        assert.equal(ratingRequests.filter(options => options.method === 'POST').length, 2);
        assert.equal(gallery.myRating(10), 5, 'The device can revise its vote');
        assert.equal(gallery.ratings[10].average, 4.9, 'The average changes');
        assert.equal(gallery.ratingVoteLabel(10), '10 votes', 'The vote count stays the same');
        gallery.sortOrder = 'rating-asc'; await Vue.nextTick(); gallery._render();
        assert.deepEqual(Array.from(gallery.getBMPs, bmp => bmp.id), [3, 10], 'Sort matching icons by lowest rating');
        const originalRatings = gallery.ratings;
        gallery.ratings = {
            3: { average: 5, count: 2 },
            10: { average: 5, count: 10 },
        };
        assert.deepEqual(Array.from(gallery.getBMPs, bmp => bmp.id), [3, 10], 'Fewer votes sort lower at the same average');
        gallery.sortOrder = 'rating-desc';
        assert.deepEqual(Array.from(gallery.getBMPs, bmp => bmp.id), [10, 3], 'More votes sort higher at the same average');
        gallery.sortOrder = 'rating-asc';
        gallery.ratings = originalRatings;
        await Vue.nextTick();
        assert.equal(galleryComponent.data().sortOrder, 'rating-asc', 'Restore rating sort order');
        assert.equal(stored.size, 1, 'Store only gallery preferences');
        gallery.$store.state.socket.isConnected = false;
        await Vue.nextTick();
        await gallery.rateBitmap(10, 3);
        assert.equal(ratingRequests.filter(options => options.method === 'POST').length, 2, 'Offline browsing cannot vote');
        await new Promise(resolve => setImmediate(resolve));
        assert.equal(gallery.myRating(10), 0, 'Offline browsing shows only the aggregate');
        assert.equal(gallery.ratings[10].average, 4.9);
        assert.equal(gallery.ratingVoteLabel(10), '10 votes');

        gallery.$store.state.socket.isConnected = true;
        gallery.$store.state.deviceUuid = 'b'.repeat(40);
        await new Promise(resolve => setImmediate(resolve));
        assert.equal(gallery.myRating(10), 0, 'Another device has no personal vote');
        gallery.$store.state.deviceUuid = uuid;
        await new Promise(resolve => setImmediate(resolve));
        assert.equal(gallery.myRating(10), 5, 'The original device retains its updated vote');
        ratingsFailure = 404;
        await gallery.loadRatings();
        assert.match(gallery.ratingLoadError, /version does not provide icon ratings/);
        const unavailableGallery = gallery._render();
        const findNodes = (node, predicate) => [
            ...(predicate(node) ? [node] : []),
            ...(node.children || node.componentOptions?.children || []).flatMap(child => findNodes(child, predicate)),
        ];
        const ratingGroups = tree => findNodes(tree, node => node.data?.attrs?.role === 'group');
        const voteButtons = tree => findNodes(tree, node => node.data?.staticClass === 'gallery-rate-button');
        assert.deepEqual(ratingGroups(unavailableGallery).map(node => node.data.attrs['aria-describedby']),
            ['rating-hint-3', 'rating-hint-10'], 'Each vote control references its own error message');
        assert.equal(findNodes(unavailableGallery, node => node.data?.staticClass === 'gallery-rating-star').length, 10,
            'Community stars remain visible when ratings are unavailable');
        assert(voteButtons(unavailableGallery).every(button => button.data.attrs.disabled));
        ratingsFailure = 'network';
        await gallery.loadRatings();
        assert.match(gallery.ratingLoadError, /Could not load ratings from PixelIt.API/);
        ratingsFailure = null;
        await gallery.loadRatings();
        assert(!gallery.ratingLoadError);
        assert(voteButtons(gallery._render()).every(button => !button.data.attrs.disabled), 'Retry restores voting');
        const [editButton] = voteButtons(gallery._render()).slice(1);
        editButton.data.on.click();
        assert.equal(gallery.openRatingBitmapId, 10);
        const editableStars = findNodes(gallery._render(), node => node.data?.attrs?.['aria-label'] === '5 stars');
        assert.equal(editableStars.length, 1);
        assert.equal(editableStars[0].data.attrs['aria-pressed'], true);
        await gallery.rateBitmap(10, 5);
        assert.equal(gallery.openRatingBitmapId, null, 'Selecting the existing vote closes inline editing');
        assert.equal(gallery.communityStarFill(10, 4), 100);
        assert(Math.abs(gallery.communityStarFill(10, 5) - 90) < 0.001, 'The last star reflects the fractional community score');
        assert.equal(gallery.communityStarFill(7, 1), 0, 'An unrated icon shows empty community stars');
        assert.equal(renderErrors.length, 0, renderErrors.map(error => error.message).join(', '));
    } finally {
        Vue.config.errorHandler = previousErrorHandler;
        gallery.$destroy();
    }
    console.log('Gallery tests passed: viewport lifecycle, resource release, search rendering, result boundaries and clearing search after scrolling.');
})().catch(error => { console.error(error); process.exitCode = 1; });
