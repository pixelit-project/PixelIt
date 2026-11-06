const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const code = fs.readFileSync(path.join(__dirname, '../src/utils/sensorPublish.js'), 'utf8').replace(/export function/g, 'function');
const [validate, validateMatrix] = new Function(code + '\nreturn [sensorPublishError, matrixInfoIntervalError];')();
const defaults = { periodic: false, interval: 60, minInterval: 0, maxInterval: 0, temperature: 0, humidity: 0, pressure: 0, gas: 0, battery: 0, lux: 0 };
assert.equal(validate(defaults), '');
assert.equal(validate({ ...defaults, periodic: true, temperature: 0.2, pressure: 0.5, maxInterval: 900 }), '');
for (const invalid of [
    { periodic: 'false' }, { interval: 2 }, { interval: 3.5 }, { interval: '60' },
    { minInterval: -1 }, { maxInterval: 86401 }, { minInterval: 60, maxInterval: 30 },
    { periodic: true, maxInterval: 30 }, { pressure: -0.5 }, { humidity: '' },
    { lux: Infinity }, { gas: NaN }, { battery: true }, { temperature: 1000001 },
]) assert.notEqual(validate({ ...defaults, ...invalid }), '');
assert.equal(validate({ ...defaults, interval: 86400, minInterval: 86400, maxInterval: 86400 }), '');
assert.equal(validate({ ...defaults, dynamic: false, minInterval: 120, maxInterval: 60 }), '');
assert.notEqual(validate({ ...defaults, dynamic: true, minInterval: 120, maxInterval: 60 }), '');
assert.notEqual(validate({ ...defaults, dynamic: 'false' }), '');
for (const valid of [undefined, 10, 15, 86400]) assert.equal(validateMatrix(valid), '');
for (const invalid of [9, 10.5, '15', 86401, '']) assert.notEqual(validateMatrix(invalid), '');
console.log('Sensor publishing UI validation passed');
