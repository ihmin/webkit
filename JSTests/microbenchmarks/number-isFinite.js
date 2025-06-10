(function () {
  var result = 0;
  var values = [
    0,
    -1,
    1,
    123.45,
    -123.45,
    Number.MIN_VALUE,
    Number.MAX_VALUE,
    Number.MAX_SAFE_INTEGER,
    Number.MIN_SAFE_INTEGER,
    Infinity,
    NaN,
  ];
  for (var i = 0; i < testLoopCount; ++i) {
    for (var j = 0; j < values.length; ++j) {
      if (Number.isFinite(values[j]))
        result++;
    }
  }
  if (result !== testLoopCount * 9)
    throw "Error: bad result: " + result;
})();
