#include <iostream>

extern "C" {
#include "main.h"
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

struct buffer_data
{
  uint8_t *ptr; /* 文件中对应位置指针 */
  size_t size;  ///< size left in the buffer /* 文件当前指针到末尾 */
  size_t pos;
};

AVFormatContext *fmt_ctx = NULL;
AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
int width, height;
enum AVPixelFormat pix_fmt;
AVStream *video_stream = NULL;
const char *video_dst_filename = NULL;

int video_stream_idx = -1;
AVFrame *frame = NULL;
AVPacket *pkt;
int video_frame_count = 0;

AVIOContext *avio_ctx = NULL;
struct buffer_data bd = {0};

void testPrint(char *str)
{
  std::cout << str << '\n';
}

void testBuffer(uint8_t *buf222, int len222)
{
  char str[len222];

  // buf222 = &buf222[1];
  for (int i = len222 - 1000; i < len222; i++)
  {
    std::cout << (int)buf222[i] << " ";
  }
  std::cout << std::endl;
}

//用来将内存buffer的数据拷贝到buf
int read_packet(void *opaque, uint8_t *buf, int buf_size)
{

  int len = fmin(bd.size - bd.pos, buf_size);
  if (!len)
  {
    return AVERROR_EOF;
  }

  memcpy(buf, bd.ptr + bd.pos, len);
  bd.pos += len;

  return buf_size;
}

int64_t seek(void *opaque, int64_t offset, int whence)
{

  if (whence == AVSEEK_SIZE)
    return bd.size;

  switch (whence)
  {
  case SEEK_SET:
    bd.pos = offset;
    break;
  case SEEK_CUR:
    bd.pos += offset;
    break;
  case SEEK_END:
    bd.pos = bd.size + offset;
    break;
  }

  bd.pos = fmax(0, fmin(bd.size, bd.pos));

  return bd.pos;
}

/* 打开前端传来的视频buffer */
int open_input_buffer(uint8_t *buf, int len)
{
  unsigned char *avio_ctx_buffer = NULL;
  size_t avio_ctx_buffer_size = 32768;

  AVInputFormat *in_fmt = av_find_input_format("h265");

  bd.ptr = buf;  /* will be grown as needed by the realloc above */
  bd.size = len; /* no data at this point */

  fmt_ctx = avformat_alloc_context();

  avio_ctx_buffer = (unsigned char *)av_malloc(avio_ctx_buffer_size);

  /* 读内存数据 */
  avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 0, NULL, read_packet, NULL, seek);

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

int testa(uint8_t *buf, int len, void (*f)(uint8_t *rbg, int width, int height))
{
  int ret;

  open_input_buffer(buf, len);

  if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
  {
    puts("avformat_find_stream_info失败.");
    return -1;
  }
  puts("avformat_find_stream_info...");

  //获取视频流的索引位置
  video_stream_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

  if (video_stream_idx == -1)
  {
    printf("%s", "找不到视频流\n");
    return -1;
  }

  video_stream = fmt_ctx->streams[video_stream_idx];
  AVCodec *video_dec = avcodec_find_decoder(video_stream->codecpar->codec_id);

  video_dec_ctx = avcodec_alloc_context3(video_dec);
  avcodec_parameters_to_context(video_dec_ctx, video_stream->codecpar);

  if (video_dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO || video_dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
  {
    if (video_dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
      video_dec_ctx->framerate = av_guess_frame_rate(fmt_ctx, video_stream, NULL);
    /* Open decoder */
    avcodec_open2(video_dec_ctx, video_dec, NULL);
  }

  //打印信息
  av_dump_format(fmt_ctx, 0, NULL, 0);

  frame = av_frame_alloc();
  pkt = av_packet_alloc();
  struct SwsContext *img_convert_ctx;
  AVFrame *frame_rgb;
  uint8_t *out_buffer;

  //为每帧图像分配内存
  frame_rgb = av_frame_alloc();

  //获得帧图大小
  out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, video_dec_ctx->width, video_dec_ctx->height, 1));

  av_image_fill_arrays(frame_rgb->data, frame_rgb->linesize, out_buffer,
                       AV_PIX_FMT_RGB24, video_dec_ctx->width, video_dec_ctx->height, 1);

  img_convert_ctx = sws_getContext(video_dec_ctx->width, video_dec_ctx->height, AV_PIX_FMT_RGB24,
                                   video_dec_ctx->width, video_dec_ctx->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

  int i = 0;

  while ((ret = av_read_frame(fmt_ctx, pkt)) >= 0)
  {
    if (pkt->stream_index == video_stream_idx)
    {
      avcodec_send_packet(video_dec_ctx, pkt);
      ret = avcodec_receive_frame(video_dec_ctx, frame);
      switch (ret)
      {
      case 0: //成功
        sws_scale(img_convert_ctx, frame->data, frame->linesize, 0, video_dec_ctx->height,
                  frame_rgb->data, frame_rgb->linesize);

        (*f)(frame_rgb->data[0], video_dec_ctx->width, video_dec_ctx->height);
        break;

      case AVERROR_EOF:
        printf("the decoder has been fully flushed,\
                           and there will be no more output frames.\n");
        break;

      case AVERROR(EAGAIN):
        printf("Resource temporarily unavailable\n");
        break;

      case AVERROR(EINVAL):
        printf("Invalid argument\n");
        break;
      default:
        break;
      }

      i++;
    }
    av_packet_unref(pkt);
  }

  sws_freeContext(img_convert_ctx);

  avcodec_free_context(&video_dec_ctx);
  avformat_close_input(&fmt_ctx);

  if (avio_ctx)
  {
    av_freep(&avio_ctx->buffer);
    av_freep(&avio_ctx);
  }

  av_frame_free(&frame);
  return i;
}