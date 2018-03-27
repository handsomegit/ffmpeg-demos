importScripts("main.js");
var Module = {}

ffmpeg(Module).then((Module) => {
    console.log('webassembly加载完毕')
    Module = Module;
    console.log(Module);
    //    postMessage({ action: "loaded" });

});

function hello() {
    console.log("hello")

    fetch("frag_bunny.mp4").then((a) => {
        return a.arrayBuffer()
    }).then((buffer) => {
        let aaa = new Uint8Array(buffer)


    })

    Module._hello();
}

function bbb() {
    console.log("bbb")
    let i = 0;
    var pointer = Module.addFunction(function (a, b, c) {
        var array = Module.HEAP8.subarray(a, b * c * 3);
        if (i++ === 1000) {
            var typedArray = new Uint8Array(array.buffer);
            console.log(b, c);

            postMessage(typedArray);

        }
        //  console.log(typedArray);
        //postMessage(array[8]);
    });


    fetch("frag_bunny.mp4").then((a) => {

        return a.arrayBuffer()
    }).then((arrayBuffer) => {

        var typedArray = new Uint8Array(arrayBuffer);

        var numBytes = typedArray.length * typedArray.BYTES_PER_ELEMENT;
        var ptr = Module._malloc(numBytes);
        var heapBytes = new Uint8Array(Module.HEAPU8.buffer, ptr, numBytes);
        heapBytes.set(new Uint8Array(typedArray.buffer));
        console.log(heapBytes.byteOffset);


        let testa = Module.cwrap('testa', 'number', ['number', 'number', 'number'])
        let asdasd = testa(heapBytes.byteOffset, typedArray.length, pointer);

        console.log(asdasd)

        Module._free(heapBytes.byteOffset);
        Module._free(asdasd.byteOffset);
    })

}

var testPrint = str => Module.ccall('testPrint',
    null, // return type
    ['string'], // argument types
    [str]);



//接收主线程消息
onmessage = (e) => {
    console.log(e);
    // hello();
    if (e.data === "bbb")
        bbb();
}
