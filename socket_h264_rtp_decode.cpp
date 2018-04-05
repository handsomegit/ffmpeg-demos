#include "stdafx.h"

#include <iostream>
#include <winsock2.h>
#include <windows.h>
#include <WS2tcpip.h>
#include "H264FrameUnpack.h"

#pragma comment(lib, "ws2_32.lib")
extern "C" {
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}


#include "WasmFFmpeg.h"

/* rtp包头结构 */
typedef struct
{
	/**//* byte 0 */
	unsigned char csrc_len : 4;        /**//* expect 0 */
	unsigned char extension : 1;        /**//* expect 1, see RTP_OP below */
	unsigned char padding : 1;        /**//* expect 0 */
	unsigned char version : 2;        /**//* expect 2 */
										  /**//* byte 1 */
	unsigned char payload : 7;        /**//* RTP_PAYLOAD_RTSP */
	unsigned char marker : 1;        /**//* expect 1 */
										 /**//* bytes 2, 3 */
	unsigned short seq_no;
	/**//* bytes 4-7 */
	unsigned  long timestamp;
	/**//* bytes 8-11 */
	unsigned long ssrc;            /**//* stream number is used here. */
} RTP_FIXED_HEADER;


//static const uint8_t start_sequence[] = { 0, 0, 0, 1 };

typedef struct {
	uint8_t *ptr;
	int size;
}start_sequence;


boolean get_payload(char * in_buf, int in_len, char * out_buf, int & out_len)
{
	int rtp_head_len;
	//char nalu[4] = { 0x00, 0x00, 0x00, 0x01 };

	//memcpy(out_buf, nalu, 4);
	rtp_head_len = sizeof(RTP_FIXED_HEADER);

	//out_buf += 4;
	in_buf += rtp_head_len;
	memcpy(out_buf, in_buf, in_len - rtp_head_len);
	out_len = in_len - rtp_head_len;

	return TRUE;
}



AVCodec * codec = NULL;
AVCodecContext * dec_ctx = NULL;
AVCodecParserContext *dec_parser_ctx;
AVPacket * pkt;
AVFrame * frame;

uint8_t rtp_frame[1024 * 1024];
int rtp_frame_size = 0;


int decode_rtp_packet2(unsigned char * buf, int len) {

	if ((NULL == buf) || (len < 12))
		return -1;
	int ret = 0;

	// 去除长度为12字节RTP头，得到NALU
	unsigned char * rtp_frame = buf + 12;
	unsigned int rtp_frame_size = len - 12;

	return 0;
}


int decode_rtp_packet(unsigned char * buf, int len)
{
	if ((NULL == buf) || (len<12))
		return -1;
	int ret = 0;

	// 去除长度为12字节RTP头，得到NALU
	unsigned char *payload = buf + 12;
	unsigned int payload_size = len - 12;

	uint8_t nal;

	/*
	SPS: 0x67
	PPS: 0x68
	SEI: 0x06
	IDR 帧: 0x65
	P 帧: 0x61 和 0x41
	*/
	uint8_t nal_header;
	uint8_t nal_type;

	nal = payload[0];
	nal_type = nal & 0x1f;
	nal_header = (payload[0] & 0xe0) | (payload[1] & 0x1f); // 0x65 为 IDR 帧


	start_sequence start_seq;

	uint8_t start_seq1[] = { 0,0,0,1 };
	uint8_t start_seq2[] = { 0,0,1 };

	// SEI IDR帧专用


	if (nal == 0x06 || nal_header == 0x65) {
		start_seq.ptr = start_seq2;
		start_seq.size = 3;
	}
	else
	{
		start_seq.ptr = start_seq1;
		start_seq.size = 4;
	}



	// 单个单元
	if (nal_type >= 0 && nal_type <= 23) {
		memcpy(rtp_frame, start_seq.ptr, start_seq.size);
		memcpy(rtp_frame + start_seq.size, payload, payload_size);
		rtp_frame_size = payload_size + start_seq.size;
	}

	// Fragmentation Units (分片单元)  FU-A 
	else if (nal_type == 28)
	{
		uint8_t start_bit = payload[1] >> 7;
		uint8_t end_bit = (payload[1] & 0x40) >> 6;

		//first pkt
		if (start_bit)
		{
			//std::cout << std::hex << static_cast<int>(nal_header) << std::endl;

			memcpy(rtp_frame, start_seq.ptr, start_seq.size);
			memcpy(rtp_frame + start_seq.size, &nal_header, 1);
			memcpy(rtp_frame + start_seq.size + 1, payload + 2, payload_size - 2);

			rtp_frame_size = start_seq.size + 1 + payload_size - 2;
			ret = -1;
		}
		// last pkt
		else if (end_bit)
		{
			memcpy(rtp_frame + rtp_frame_size, payload + 2, payload_size - 2);
			rtp_frame_size = rtp_frame_size + payload_size - 2;
		}
		// middle pkt
		else
		{
			memcpy(rtp_frame + rtp_frame_size, payload + 2, payload_size - 2);
			rtp_frame_size = rtp_frame_size + payload_size - 2;
			ret = -1;

		}
	}
	else
	{
		rtp_frame_size = 0;
	}


	return ret;
}

int init_decoder() {

	codec = avcodec_find_decoder(AV_CODEC_ID_H264);


	dec_ctx = avcodec_alloc_context3(codec);


	int ret = avcodec_open2(dec_ctx, codec, NULL);
	if (ret != 0) {
		return 1;
	}

	dec_parser_ctx = av_parser_init(AV_CODEC_ID_H264);


	pkt = av_packet_alloc();
	frame = av_frame_alloc();
	return 0;
}

int main()
{
	

	WSADATA wsaData;
	WORD sockVersion = MAKEWORD(2, 2);
	if (WSAStartup(sockVersion, &wsaData) != 0) {
		return 0;
	}

	SOCKET serSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (serSocket == INVALID_SOCKET) {
		printf("socket error !");
		return 0;
	}

	sockaddr_in serAddr;
	serAddr.sin_family = AF_INET;
	serAddr.sin_port = htons(8888);
	serAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(serSocket, (sockaddr *)& serAddr, sizeof(serAddr)) == SOCKET_ERROR) {
		printf("bind error !");
		closesocket(serSocket);
		return 0;
	}

	sockaddr_in remoteAddr;
	int nAddrLen = sizeof(remoteAddr);


	FILE * fp = NULL;
	fopen_s(&fp, "test2222222222.264", "wb");

	int frame_count = 0;

	int ret;
	init_decoder();

	int paser_len;
	while (1) {

		char recv_data[2048];
		int len = recvfrom(serSocket, recv_data, 2048, 0, (sockaddr *)& remoteAddr, &nAddrLen);
		if (len > 0) {
			recv_data[len] = 0x00;

			// 组帧
			ret = decode_rtp_packet((unsigned char *)& recv_data, len);

			//av_parser_parse2

			if (ret == 0) {
			
				fwrite(rtp_frame, 1, rtp_frame_size, fp);
			

				paser_len = av_parser_parse2(dec_parser_ctx, dec_ctx, &pkt->data, &pkt->size, rtp_frame, rtp_frame_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);

				if (paser_len == 0) {
					frame_count++;
					printf("frame: %d ", frame_count);
					std::cout << std::hex << static_cast<int>(pkt->data[4]) << " ";
					std::cout << std::hex << static_cast<int>(pkt->data[5]) << " ";
					std::cout << std::hex << static_cast<int>(pkt->data[6]) << " ";
					std::cout << std::hex << static_cast<int>(pkt->data[7]) << " ";
				
					avcodec_send_packet(dec_ctx, pkt);

					ret = avcodec_receive_frame(dec_ctx, frame);


					if (ret == 0) {
					//	std::cout << paser_len << std::endl;
						printf("got sucess! \n");
					}
					else {
						
			
						printf("got fail... \n");

					}

				}
				av_packet_unref(pkt);


			}

		}


	}

	fclose(fp);
	av_frame_free(&frame);
	closesocket(serSocket);
	WSACleanup();
	return 0;

}
