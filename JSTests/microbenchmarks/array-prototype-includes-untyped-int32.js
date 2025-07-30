function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(array, searchElement) {
    return array.includes(searchElement);
}
noInline(test);

const array = new Array(1024);
for (let i = 0; i < array.length; i++) {
    array[i] = i;
}

for (let i = 0; i < 1e5; i++) {
    const odd = i % 2 === 0;
    const searchElement = odd ? 512 : {};
    shouldBe(test(array, searchElement), odd ? true : false);
}
