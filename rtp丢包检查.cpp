int preSeq = 0xFFFF;

int check(uint8_t *buf) {
	int	seq = buf[2] << 8 | buf[3];
	if (seq > preSeq) {
		printf("丢包 seq %d \n", seq);
	}
	preSeq = seq;
	return seq > preSeq;
}
