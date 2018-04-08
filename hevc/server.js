

const net = require('net');
var rtsp = require('rtsp-stream')
var transform = require('sdp-transform');
const fs = require('fs');

const querystring = require('querystring');
var HOST = '172.30.173.10';
var PORT = 554;
var url = 'rtsp://172.30.173.10:554/frag_bunny.265';
var userAgent = 'rtspclient/1.0.0';
var session;
var server_tcp_port;
var server_rtcp_port;

// 视频的宽度高度信息 base64编码
var sprop_parameter_sets;


var server = require('http').createServer();
var io = require('socket.io')(server);
io.on('connection', function(client){
  client.on('event', function(data){});
  client.on('disconnect', function(){});
});
server.listen(3003);


setTimeout(() => {
    /*rtpServer*/
const dgram = require('dgram');
const rtpServer = dgram.createSocket('udp4'); //创建udp服务器
const multicastAddr = '224.100.100.100';


//以下rtpServer.on 都是在监听不同信号
rtpServer.on('close', () => { // ()=> 是 ES6的箭头函数，写成 function()也是可以的
    console.log('socket已关闭');
});

rtpServer.on('error', (err) => {
    console.log(err);
});

rtpServer.on('listening', () => {
    console.log('socket正在监听中...');
    rtpServer.addMembership(multicastAddr); //加入组播组
    rtpServer.setMulticastTTL(128);
});
var stream = fs.createWriteStream('h265.265');

rtpServer.on('message', (msg, rinfo) => {
    console.log(`receive message from ${rinfo.address}:${rinfo.port}：${rinfo.size}`);


stream.write(msg);
    console.log(msg);

   sendMsg(msg, rinfo.size);

   io.send(msg,rinfo.size);
});

function sendMsg(msg, size) {
    rtpServer.send(msg, 0, size, 8888, "127.0.0.1");
}


function rtpSendMsg() {
    var message = `GET_PARAMETER ${url} RTSP/1.0\r\
    n
    CSeq: 5\r\n
    User-Agent: ${userAgent}\r\n
    Session: ${session}\r\n
    x-Timeshift_Range:\r\n
    \r\n`;
    rtpServer.send(message, 0, message.length, server_tcp_port, HOST);

    // 心跳
    setInterval(() => {
        rtpServer.send(message, 0, message.length, server_tcp_port, HOST);

    }, 50000);

    //通过rtpServer.send发送组播
    //参数分别是，数据（buffer或者string），偏移量（即开始发送的位子），数据长度，接收的端口，组播组
}

rtpServer.bind(8028); //绑定端口，不绑定的话也可以send数据但是无法接受

const rtcpServer = dgram.createSocket('udp4'); //创建udp服务器
rtcpServer.on('listening', () => {
    console.log('rtcp正在监听中...');
    rtcpServer.addMembership(multicastAddr);
});
rtcpServer.on('message', (msg, rinfo) => {
    console.log(`receive message from ${rinfo.address}:${rinfo.port}：${msg}`);
});
rtcpServer.bind(8029); //绑定端口，不绑定的话也可以send数据但是无法接受


var decoder = new rtsp.Decoder()
var encoder = new rtsp.Encoder()

var body = '\r\n'
var options = {
    method: 'OPTIONS',
    uri: url,
    headers: {
        CSeq: 1,
        'User-Agent:': 'rtspclient/1.0.0',
        'Content-Length': Buffer.byteLength(body)
    },
    body: body
}

decoder.on('response', function (res) {
    // console.log("-------------------------------header------------------------------------")
    // console.log(res.headers)
    // console.log("--------------------------------header-----------------------------------\n")


    if (res.headers.cseq && res.headers.cseq === '1') {

        // 第二步 发送DESCRIBE
        let req = encoder.request({ method: 'DESCRIBE', uri: url })
        req.setHeader('CSeq', 2);
        req.setHeader('User-Agent:', 'rtspclient/1.0.0');
        req.setHeader('Accept', 'application/sdp');

        req.end()

    }

    if (res.headers.cseq && res.headers.cseq === '2') {

        let sdp;
        res.on('data', (chunk) => {
            let sdpStr = chunk.toString();
            sdp = transform.parse(sdpStr);
        });

        res.on('end', (s) => {
            // 第三步 发送SETUP

            let control = sdp.media[0].control;
            let uri = control.indexOf("rtsp://") >= 0 ? control : url + "/" + control;

            let req = encoder.request({ method: 'SETUP', uri })
            req.setHeader('CSeq', 3);
            req.setHeader('User-Agent:', 'rtspclient/1.0.0');
            req.setHeader('Transport', `RTP/AVP/UDP;unicast;client_port=8028-8029`);

            req.end()
        });

    }

    if (res.headers.cseq && res.headers.cseq === '3') {
        let transport = querystring.parse(res.headers.transport, ";", "=")
        let server_port = transport.server_port.split("-");
        server_tcp_port = server_port[0];
        server_rtcp_port = server_port[1];
        console.log(server_tcp_port);

        session = session = res.headers.session.split(";")[0];
        // 第四步 发送PLAY
        let req = encoder.request({ method: 'PLAY', uri: url })
        req.setHeader('Range', "Range: npt=0.000-");
        req.setHeader('CSeq', 4);
        req.setHeader('User-Agent:', 'rtspclient/1.0.0');
        req.setHeader('Session', session);

        req.end();

        // 执行这步后，rtsp服务端开始推流到udp 只能放这里，不然rtsp服务器不会发送sdp ssp sei包
        rtpSendMsg();
    }

    if (res.headers.cseq && res.headers.cseq === '4') {
       
    }


})



var client = new net.Socket();

client.connect(PORT, HOST, () => {
    // client.write(`OPTIONS rtsp://172.30.173.7:554/frag_bunny.264 RTSP/1.0\r\n
    //               CSeq: 1\r\n
    //               User-Agent: rtspclient/1.0.0\r\n
    //               \r\n`);

    client.pipe(decoder)

    encoder.pipe(client);



    encoder.request(options, function () {
        console.log('done sending request 1 to client')
    });

});

// 为客户端添加“data”事件处理函数
// data是服务器发回的数据
client.on('data', function (data) {
    // console.log(data.toString());
    // 完全关闭连接
    // client.destroy();
});

// 为客户端添加“close”事件处理函数
client.on('close', function () {
    console.log('Connection closed');
});


}, 0);

