bool save_pic(AVFrame *frm, AVPixelFormat pfmt, AVCodecID cid, const char* filename, int width, int height)
{
	int outbuf_size = width * height * 4;
	uint8_t * outbuf = (uint8_t*)malloc(outbuf_size);
	int got_pkt = 0;

fopen_s(&pf,filename, "wb");
	if (pf == NULL)
		return false;
	AVPacket pkt;
	AVCodec *pCodecRGB24;
	AVCodecContext *ctx = NULL;
	pCodecRGB24 = avcodec_find_encoder(cid);
	if (!pCodecRGB24)
		return false;
	ctx = avcodec_alloc_context3(pCodecRGB24);
	ctx->bit_rate = 3000000;
	ctx->width = width;
	ctx->height = height;
	AVRational rate;
	rate.num = 1;
	rate.den = 25;
	ctx->time_base = rate;
	ctx->gop_size = 10;
	ctx->max_b_frames = 0;
	ctx->thread_count = 1;
	ctx->pix_fmt = pfmt;


	int ret = avcodec_open2(ctx, pCodecRGB24, NULL);
	if (ret < 0)
		return false;

	//  int size = ctx->width * ctx->height * 4;  
	av_init_packet(&pkt);
	static int got_packet_ptr = 0;
	pkt.size = outbuf_size;
	pkt.data = outbuf;

	got_pkt = avcodec_send_frame(ctx, frame);

	got_pkt = avcodec_receive_packet(ctx, &pkt);

	frm->pts++;
	if (got_pkt == 0)
	{
		fwrite(pkt.data, 1, pkt.size, pf);
	}
	else
	{
		return false;
	}
	fclose(pf);
	return true;
}
//AV_PIX_FMT_YUVJ420P+AV_CODEC_ID_MJPEG!!!
int main (){
		save_pic(frame, AV_PIX_FMT_YUVJ420P, AV_CODEC_ID_MJPEG, fname, dec_ctx->width, dec_ctx->height);

}
