const assert = require('assert');
const fs = require('fs');
const vm = require('vm');
const path = require('path');
const context = { UserMap: {} };
const source = fs.readFileSync(path.join(__dirname, '../src/views/Statistics.vue'), 'utf8')
    .split('<script>')[1].split('</script>')[0].replace(/^import .*;$/gm, '').replace('export default', 'this.component =');
vm.runInNewContext(source, context);
const page = { $vuetify: { theme: { dark: true } }, $store: { state: { statistics: {} } } };
const options = () => [context.component.computed.chartOptions.call(page), context.component.computed.countryChartOptions.call(page)];
const expected = ['#A300D6', '#7D02EB', '#5653FE', '#2983FF', '#00B1F2'];
for (const dark of [true, false, true]) {
    page.$vuetify.theme.dark = dark;
    for (const countryStats of [undefined, [{ country: 'Germany', count: 30 }, { country: 'France', count: 10 }]]) {
        page.$store.state.statistics.countryStats = countryStats;
        const [bars, countries] = options();
        assert.deepEqual(Array.from(bars.colors), expected);
        assert.deepEqual(Array.from(countries.colors), expected);
        assert.equal(countries.theme.mode, dark ? 'dark' : 'light');
        assert.equal(countries.labels.length, countryStats ? 2 : 0);
        // ApexCharts mutates option arrays internally; each chart must own its copy.
        countries.colors.push('#ffffff');
        assert.equal(bars.colors.length, 5);
        assert.equal(options()[1].colors.length, 5);
    }
}
console.log('Statistics colors remain consistent across async data and theme changes');
