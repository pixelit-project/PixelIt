<template>
    <div class="gallery-viewport-item" :style="{ height: active ? '100%' : placeholderHeight + 'px' }" :aria-hidden="active ? null : 'true'">
        <slot v-if="active"></slot>
    </div>
</template>

<script>
// One observer for the whole gallery, rather than one per retained placeholder.
const items = new Map();
let observer;

export default {
    props: {
        pinned: { type: Boolean, default: false },
    },
    data() {
        return { active: false, nearby: false, placeholderHeight: 330 };
    },
    mounted() {
        if (typeof IntersectionObserver === 'undefined') {
            this.active = true;
            return;
        }
        if (!observer) {
            observer = new IntersectionObserver((entries) => {
                entries.forEach((entry) => {
                    const item = items.get(entry.target);
                    if (item) {
                        item.nearby = entry.isIntersecting;
                        item.updateContent();
                    }
                });
            }, { rootMargin: '1200px 0px' });
        }
        // Mount nearby contents in the same render cycle after a filter reset,
        // without waiting for the asynchronous observer notification.
        const rect = this.$el.getBoundingClientRect();
        this.nearby = rect.bottom >= -1200 && rect.top <= window.innerHeight + 1200;
        this.updateContent();
        items.set(this.$el, this);
        observer.observe(this.$el);
    },
    beforeDestroy() {
        if (observer) observer.unobserve(this.$el);
        items.delete(this.$el);
        if (!items.size && observer) {
            observer.disconnect();
            observer = null;
        }
    },
    watch: {
        pinned() {
            this.updateContent();
        },
    },
    methods: {
        updateContent() {
            const active = this.nearby || this.pinned;
            if (this.active && !active) {
                // Measure before unmounting to retain the row's space and scroll position.
                this.placeholderHeight = this.$el.getBoundingClientRect().height;
            }
            this.active = active;
        },
    },
};
</script>

<style scoped>
.gallery-viewport-item {
    min-height: 330px;
    border-radius: 10px;
    background: rgba(128, 128, 128, 0.08);
}
</style>
