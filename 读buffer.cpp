


//用来将内存buffer1的数据拷贝到buf
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

/* 打开前端传来的视频buffer */
int open_input_buffer(uint8_t *buf, int len)
{
  AVIOContext *avio_ctx = NULL;
  unsigned char *avio_ctx_buffer = NULL;
  size_t avio_ctx_buffer_size = 32768;
  struct buffer_data bd = {0};

  bd.ptr = buf;  /* will be grown as needed by the realloc above */
  bd.size = len; /* no data at this point */

  fmt_ctx = avformat_alloc_context();

  avio_ctx_buffer = (unsigned char *)av_malloc(avio_ctx_buffer_size);

  /* 读内存数据 */
  avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 0, &bd, read_packet, NULL, NULL);

  fmt_ctx->pb = avio_ctx;

  /* 打开内存缓存文件, and allocate format context */
  if (avformat_open_input(&fmt_ctx, NULL, NULL, NULL) < 0)
  {
    fprintf(stderr, "Could not open input\n");
    return -1;
  }
  return 0;
}
