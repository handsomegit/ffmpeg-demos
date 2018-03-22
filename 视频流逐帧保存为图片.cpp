#include <iostream>
#include <string.h>
#include <sstream>
#include <windows.h>  

#define __STDC_CONSTANT_MACROS

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

int get_format_from_sample_fmt(const char **fmt, enum AVSampleFormat sample_fmt)
{
	int i;
	struct sample_fmt_entry {
		enum AVSampleFormat sample_fmt;
		const char *fmt_be, *fmt_le;
	}

	sample_fmt_entries[] = {
		{ AV_SAMPLE_FMT_U8,  "u8",    "u8" },
	{ AV_SAMPLE_FMT_S16, "s16be", "s16le" },
	{ AV_SAMPLE_FMT_S32, "s32be", "s32le" },
	{ AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
	{ AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
	};

	*fmt = NULL;

	for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
		struct sample_fmt_entry *entry = &sample_fmt_entries[i];
		if (sample_fmt == entry->sample_fmt) {
			*fmt = AV_NE(entry->fmt_be, entry->fmt_le);
			return 0;
		}
	}

	fprintf(stderr,
		"sample format %s is not supported as output format\n",
		av_get_sample_fmt_name(sample_fmt));
	return -1;
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

void demuxing_decoding(const char* filename)
{

	AVFormatContext* formatContext = nullptr;


	if (0 != avformat_open_input(&formatContext, filename, NULL, NULL))
	{
		puts("avformat_open_input失败.");
		return;
	}
	puts("avformat_open_input...");

	if (avformat_find_stream_info(formatContext, NULL) < 0)
	{
		puts("avformat_find_stream_info失败.");
		return;
	}
	puts("avformat_find_stream_info...");


	//获取视频流的索引位置
	//遍历所有类型的流（音频流、视频流、字幕流），找到视频流
	int video_stream_idx = -1;

	//number of streams
	for (int i = 0; i < formatContext->nb_streams; i++)
	{
		//流的类型
		if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
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

	AVStream* video_st = formatContext->streams[video_stream_idx];
	AVCodec *videCodec = avcodec_find_decoder(video_st->codecpar->codec_id);
	AVCodecContext* videoContext = avcodec_alloc_context3(videCodec);
	avcodec_parameters_to_context(videoContext, video_st->codecpar);
	avcodec_open2(videoContext, videCodec, nullptr);

	uint8_t *video_dst_data[4] = { NULL };
	int video_dst_linesize[4];
	int video_dst_bufsize = av_image_alloc(video_dst_data, video_dst_linesize, videoContext->width, videoContext->height, videoContext->pix_fmt, 1);

	//音频
	int audio_stream_idx = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	AVStream* audio_st = formatContext->streams[audio_stream_idx];
	AVCodec* audioCodec = avcodec_find_decoder(audio_st->codecpar->codec_id);
	AVCodecContext* audioContext = avcodec_alloc_context3(audioCodec);
	avcodec_parameters_to_context(audioContext, audio_st->codecpar);
	avcodec_open2(audioContext, audioCodec, nullptr);

	av_dump_format(formatContext, 0, filename, 0);

	AVFrame* frame = av_frame_alloc();

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;
	std::cout << videCodec->name << "\n";
	std::cout << audioCodec->name << "\n";

	FILE *videofile = NULL;
	char video_name[128] = { 0 };
	sprintf_s(video_name, 128, "output\\demuxing_video.%s", videCodec->name);
	fopen_s(&videofile, video_name, "wb");

	FILE *audiofile = NULL;

	char audio_name[128] = { 0 };
	sprintf_s(audio_name, 128, "output\\demuxing_audio.%s", audioCodec->name);
	fopen_s(&audiofile, audio_name, "wb");




	struct SwsContext *img_convert_ctx;
	AVFrame *pFrame, *pFrameYUV;
	int frame_finished;
	int picture_size;
	uint8_t *out_buffer;



	//为每帧图像分配内存  
	pFrame = av_frame_alloc();
	pFrameYUV = av_frame_alloc();

	if (pFrame == NULL || pFrameYUV == NULL) {
		printf("av frame alloc failed!\n");
		exit(1);
	}

	////获得帧图大小  
	out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, videoContext->width, videoContext->height, 1));
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
		AV_PIX_FMT_RGB24, videoContext->width, videoContext->height, 1);
	std::cout << videoContext->pix_fmt << "\n";
	img_convert_ctx = sws_getContext(videoContext->width, videoContext->height, AV_PIX_FMT_RGB24,
		videoContext->width, videoContext->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);


	FILE *p_file = NULL;
	char p_name[128] = { 0 };
	sprintf_s(p_name, 128, "output\\out.rgb", audioCodec->name);
	fopen_s(&p_file, p_name, "wb");
	int                 frame_cnt = 1;


	int i = 0;
	while (av_read_frame(formatContext, &pkt) >= 0) 
	{
		if (pkt.stream_index == video_stream_idx)
		{

			avcodec_send_packet(videoContext, &pkt);
			while (0 == avcodec_receive_frame(videoContext, frame))
			{
				sws_scale(img_convert_ctx, frame->data, frame->linesize, 0, videoContext->height,
					pFrameYUV->data, pFrameYUV->linesize);
				//fwrite(pFrameYUV->data[0], (videoContext->width) * (videoContext->height) * 3, 1, p_file);

				bmp_save(pFrameYUV, videoContext->width, videoContext->height, i);
				std::cout << videoContext->width << "\n";
			
			}
			
			i++;

		}

		av_packet_unref(&pkt);
	}


	fclose(videofile);
	fclose(audiofile);
	fclose(p_file);
	sws_freeContext(img_convert_ctx);

	avcodec_free_context(&videoContext);
	avcodec_free_context(&audioContext);
	avformat_close_input(&formatContext);
	av_frame_free(&frame);
	av_free(video_dst_data[0]);

}




int main()
{
	demuxing_decoding("./frag_bunny.mp4");
}
