#include <iostream>
#include <string.h>
#include <sstream>

#define __STDC_CONSTANT_MACROS

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

void demuxing_decoding(const char* filename)
{
	puts("注册...");

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

	while (av_read_frame(formatContext, &pkt) >= 0) 
	{
		if (pkt.stream_index == video_stream_idx)
		{
			avcodec_send_packet(videoContext, &pkt);
			while (0 == avcodec_receive_frame(videoContext, frame))
			{
				av_image_copy(video_dst_data, video_dst_linesize, (const uint8_t **)frame->data, frame->linesize, videoContext->pix_fmt, videoContext->width, videoContext->height);
				fwrite(video_dst_data[0], 1, video_dst_bufsize, videofile);
				puts("写入视频");
			}
		}
		else if (pkt.stream_index == audio_stream_idx)
		{
			avcodec_send_packet(audioContext, &pkt);
			while (0 == avcodec_receive_frame(audioContext, frame))
			{
				size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample((AVSampleFormat)frame->format);
				fwrite(frame->extended_data[0], 1, unpadded_linesize, audiofile);
				puts("写入音频");
			}
		}

		av_packet_unref(&pkt);
	}

	fclose(videofile);
	fclose(audiofile);
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
