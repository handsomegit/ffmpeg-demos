#include "stdafx.h"

#include <iostream>
#include <string.h>
#include <sstream>
#include <windows.h>  
#include <curl/curl.h>
#include <curl/easy.h>
#include <cstdlib>
#define __STDC_CONSTANT_MACROS

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/file.h>

}

struct buffer_data
{
	uint8_t *ptr; /* 文件中对应位置指针 */
	size_t size;  ///< size left in the buffer /* 文件当前指针到末尾 */
};

AVFormatContext *fmt_ctx = NULL;
AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
int width, height;
enum AVPixelFormat pix_fmt;
AVStream *video_stream = NULL;
const char *video_dst_filename = NULL;


int video_stream_idx = -1;
AVFrame *frame = NULL;
AVPacket pkt;
int video_frame_count = 0;

int read_buffer(void* ptr, uint8_t* buf, int buf_size)

{

	FILE* fp = (FILE*)ptr;

	size_t size = fread(buf, 1, buf_size, fp);

	int ret = size;

	return ret;

}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct buffer_data *mem = (struct buffer_data *)userp;

	mem->ptr = (uint8_t *)realloc(mem->ptr, mem->size + realsize + 1);
	if (mem->ptr == NULL)
	{
		/* out of memory! */
		printf("not enough memory (realloc returned NULL)\n");
		return 0;
	}

	memcpy(&(mem->ptr[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->ptr[mem->size] = 0;

	return realsize;
}

buffer_data fecth_to_buffer()
{
	CURL *curl_handle;
	CURLcode res;

	struct buffer_data chunk;

	chunk.ptr = (uint8_t *)malloc(1); /* will be grown as needed by the realloc above */
	chunk.size = 0;                   /* no data at this point */

	curl_global_init(CURL_GLOBAL_ALL);

	/* init the curl session */
	curl_handle = curl_easy_init();

	/* specify URL to get */
	curl_easy_setopt(curl_handle, CURLOPT_URL, "http://10.10.1.119:8888/frag_bunny.mp4");

	/* send all data to this function  */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

	/* some servers don't like requests that are made without a user-agent
	field, so we provide one */
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

	/* get it! */
	res = curl_easy_perform(curl_handle);

	/* check for errors */
	if (res != CURLE_OK)
	{
		fprintf(stderr, "curl_easy_perform() failed: %s\n",
			curl_easy_strerror(res));
	}
	else
	{
		/*
		* Now, our chunk.memory points to a memory block that is chunk.size
		* bytes big and contains the remote file.
		*
		* Do something nice with it!
		*/

		printf("%lu bytes retrieved\n", (long)chunk.size);
	}

	/* cleanup curl stuff */
	curl_easy_cleanup(curl_handle);

	/* we're done with libcurl, so clean it up */
	curl_global_cleanup();

	return chunk;
}

//用来将内存buffer的数据拷贝到buf
int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
	struct buffer_data *bd = (struct buffer_data *)opaque;
	buf_size = FFMIN(buf_size, bd->size);

	if (!buf_size)
		return AVERROR_EOF;
	printf("ptr:%p size:%zu\n", bd->ptr, bd->size);

	/* copy internal buffer data to buf */
	memcpy(buf, bd->ptr, buf_size);
	bd->ptr += buf_size;
	bd->size -= buf_size;

	return buf_size;
}

int read_buffer2(void* ptr, uint8_t* buf, int buf_size)

{

	FILE* fp = (FILE*)ptr;

	size_t size = fread(buf, 1, buf_size, fp);
	std::cout << ptr << "\n";

	int ret = size;

	return ret;

}


/* 打开前端传来的视频buffer */
int open_input_buffer(uint8_t *buf, int len)
{
	AVIOContext *avio_ctx = NULL;
	unsigned char *avio_ctx_buffer = NULL;
	size_t avio_ctx_buffer_size = 32768;
	struct buffer_data bd = { 0 };

	AVInputFormat* in_fmt = NULL;

	bd.ptr = buf;  /* will be grown as needed by the realloc above */
	bd.size = len; /* no data at this point */

	fmt_ctx = avformat_alloc_context();

	avio_ctx_buffer = (unsigned char *)av_malloc(avio_ctx_buffer_size);

	/* 读内存数据 */
	avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 0, &bd, read_packet, NULL, NULL);



	if (av_probe_input_buffer2(avio_ctx, &in_fmt, "", NULL, 0, 0) < 0)//探测从内存中获取到的媒体流的格式
	{
		fprintf(stderr, "probe format failed\n");
		return -1;
	}
	else {
		fprintf(stdout, "format:%s[%s]\n", in_fmt->name, in_fmt->long_name);
	}

	fmt_ctx->pb = avio_ctx;
	fmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;

	/* 打开内存缓存文件, and allocate format context */
	if (avformat_open_input(&fmt_ctx, "", NULL, NULL) < 0)
	{
		fprintf(stderr, "Could not open input\n");
		return -1;
	}
	return 0;
}

int open_input_file() {
	AVIOContext *avio_ctx = NULL;
	unsigned char *avio_ctx_buffer = NULL;
	size_t avio_ctx_buffer_size = 32768;
	struct buffer_data bd = { 0 };

	AVInputFormat* in_fmt = NULL;


	FILE *p_file = NULL;
	fopen_s(&p_file, "frag_bunny265.mp4", "rb");

	fmt_ctx = avformat_alloc_context();

	avio_ctx_buffer = (unsigned char *)av_malloc(avio_ctx_buffer_size);

	/* 读内存数据 */
	avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 0, p_file, read_buffer2, NULL, NULL);



	if (av_probe_input_buffer2(avio_ctx, &in_fmt, "", NULL, 0, 0) < 0)//探测从内存中获取到的媒体流的格式
	{
		fprintf(stderr, "probe format failed\n");
		return -1;
	}
	else {
		fprintf(stdout, "format:%s[%s]\n", in_fmt->name, in_fmt->long_name);
	}

	fmt_ctx->pb = avio_ctx;
	fmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;

	/* 打开内存缓存文件, and allocate format context */
	if (avformat_open_input(&fmt_ctx, "", NULL, NULL) < 0)
	{
		fprintf(stderr, "Could not open input\n");
		return -1;
	}
	return 0;
}

static void bmp_save(AVFrame *pFrame, int width, int height, int iFrame)
{
	BITMAPFILEHEADER bmpheader;
	BITMAPINFO bmpinfo;
	int y = 0;
	FILE *pFile;
	char szFilename[32];


	FILE *p_file = NULL;
	char p_name[128] = { 0 };
	sprintf_s(p_name, 128, "output\\frame%d.bmp", iFrame);
	fopen_s(&p_file, p_name, "wb");

	bmpheader.bfType = 0x4d42;
	bmpheader.bfReserved1 = 0;
	bmpheader.bfReserved2 = 0;
	bmpheader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bmpheader.bfSize = bmpheader.bfOffBits + width * height * 24 / 8;

	bmpinfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmpinfo.bmiHeader.biWidth = width;
	bmpinfo.bmiHeader.biHeight = -height;
	bmpinfo.bmiHeader.biPlanes = 1;
	bmpinfo.bmiHeader.biBitCount = 24;
	bmpinfo.bmiHeader.biCompression = BI_RGB;
	bmpinfo.bmiHeader.biSizeImage = 0;
	bmpinfo.bmiHeader.biXPelsPerMeter = 100;
	bmpinfo.bmiHeader.biYPelsPerMeter = 100;
	bmpinfo.bmiHeader.biClrUsed = 0;
	bmpinfo.bmiHeader.biClrImportant = 0;

	fwrite(&bmpheader, sizeof(bmpheader), 1, p_file);
	fwrite(&bmpinfo.bmiHeader, sizeof(bmpinfo), 1, p_file);

	fwrite(pFrame->data[0], width*height * 24 / 8, 1, p_file);

	/*for(y=0; y<height; y++)
	{
	fwrite(pFrame->data[0] + y*pFrame->linesize[0], 1, width*3, pFile);
	}*/

	//fflush(pFile);  
	fclose(p_file);
}

buffer_data get_file_buffer(const char * filename) {
	uint8_t *buffer = NULL;
	size_t buffer_size;
 buffer_data bd = { 0 };



	/* slurp file content into buffer */
	av_file_map(filename, &buffer, &buffer_size, 0, NULL);
	bd.ptr = buffer;
	bd.size = buffer_size;

	return bd;
}



void demuxing_decoding(const char* filename)
{


	char filepath[] = "frag_bunny265.mp4";

	open_input_file();

	//buffer_data bd = get_file_buffer(filepath);
	//open_input_buffer(bd.ptr,bd.size);

	/* 打开内存缓存文件, and allocate format context */
	//if (avformat_open_input(&fmt_ctx, "http://10.10.1.119:8888/frag_bunny.mp4", NULL, NULL) < 0)
	//{
	//	fprintf(stderr, "Could not open input\n");
	//	return;
	//}

	if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
	{
		puts("avformat_find_stream_info失败.");
		return;
	}
	puts("avformat_find_stream_info...");


	//获取视频流的索引位置
	//遍历所有类型的流（音频流、视频流、字幕流），找到视频流
	int video_stream_idx = -1;

	//number of streams
	for (int i = 0; i < fmt_ctx->nb_streams; i++)
	{
		//流的类型
		if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			video_stream_idx = i;
			break;
		}
	}

	std::cout << video_stream_idx << "\n";

	if (video_stream_idx == -1)
	{
		printf("%s", "找不到视频流\n");
		return;
	}

	video_stream = fmt_ctx->streams[video_stream_idx];
	AVCodec *video_dec = avcodec_find_decoder(video_stream->codecpar->codec_id);



	video_dec_ctx = avcodec_alloc_context3(video_dec);
	avcodec_parameters_to_context(video_dec_ctx, video_stream->codecpar);
	//avcodec_open2(video_dec_ctx, video_dec, NULL);


	if (video_dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO || video_dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
		if (video_dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
			video_dec_ctx->framerate = av_guess_frame_rate(fmt_ctx, video_stream, NULL);
		/* Open decoder */
		 avcodec_open2(video_dec_ctx, video_dec, NULL);

	}
	std::cout << video_dec->name << "\n";

	//打印信息
	av_dump_format(fmt_ctx, 0, NULL, 0);

	frame = av_frame_alloc();

	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	struct SwsContext *img_convert_ctx;
	AVFrame  *frame_rgb;
	uint8_t *out_buffer;



	//为每帧图像分配内存  
	frame_rgb = av_frame_alloc();


	////获得帧图大小  
	out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, video_dec_ctx->width, video_dec_ctx->height, 1));
	av_image_fill_arrays(frame_rgb->data, frame_rgb->linesize, out_buffer,
		AV_PIX_FMT_RGB24, video_dec_ctx->width, video_dec_ctx->height, 1);
	std::cout << video_dec_ctx->pix_fmt << "\n";
	img_convert_ctx = sws_getContext(video_dec_ctx->width, video_dec_ctx->height, AV_PIX_FMT_RGB24,
		video_dec_ctx->width, video_dec_ctx->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);


	//
	//Sleep(1000);
	//

	//int i = 0;
	//int ret;
	//while ((ret = av_read_frame(fmt_ctx, &pkt)) >= 0)
	//{
	//	if (pkt.stream_index == video_stream_idx)
	//	{

	//		avcodec_send_packet(video_dec_ctx, &pkt);
	//		while (0 == avcodec_receive_frame(video_dec_ctx, frame))
	//		{
	//			sws_scale(img_convert_ctx, frame->data, frame->linesize, 0, video_dec_ctx->height,
	//				frame_rgb->data, frame_rgb->linesize);
	//			//fwrite(pFrameYUV->data[0], (videoContext->width) * (videoContext->height) * 3, 1, p_file);
	//			bmp_save(frame_rgb, video_dec_ctx->width, video_dec_ctx->height, i);

	//			std::cout << i << "\n";

	//		}

	//		i++;

	//	}

	//	av_packet_unref(&pkt);
	//}


	sws_freeContext(img_convert_ctx);

	avcodec_free_context(&video_dec_ctx);
	avformat_close_input(&fmt_ctx);
	av_frame_free(&frame);

}




int main()
{
	demuxing_decoding("./frag_bunny.mp4");
}
