#include "fp.h"
#include <cstdio>
#include <cmath>


void fp16_gamma_table()
{
	for (int i = 0; i < 0x8000; i++) {
		uint16_t t = i;
		if (fp16_is_zero(t)) {
			t = FP16_P_ZERO;
		} else if (fp16_is_nan(t)) {
			// nop
		} else if (fp16_is_inf(t)) {
			// nop
		} else {
			float f = fp16_to_fp32(t);
			f = powf(f, 1 / 2.2f);
			t = fp32_to_fp16(f);
		}
		printf("0x%04x,", t);
		if ((i + 1) % 8 == 0) {
			putchar('\n');
		}
	}
}

void fp16_degamma_table()
{
	for (int i = 0; i < 0x8000; i++) {
		uint16_t t = i;
		if (fp16_is_zero(t)) {
			t = FP16_P_ZERO;
		} else if (fp16_is_nan(t)) {
			// nop
		} else if (fp16_is_inf(t)) {
			// nop
		} else {
			float f = fp16_to_fp32(t);
			f = powf(f, 2.2f);
			t = fp32_to_fp16(f);
		}
		printf("0x%04x,", t);
		if ((i + 1) % 8 == 0) {
			putchar('\n');
		}
	}
}

int main()
{
	for (int i = 0; i < 256; i++) {
		float f = i / 255.0f;
		uint16_t g = fp32_to_fp16(f);
		g = fp16_degamma(g);
		g = fp16_gamma(g);
		f = fp16_to_fp32(g);
		printf("0x%02x,", (int)(f * 255.0f + 0.5f));
		if ((i + 1) % 16 == 0) {
			putchar('\n');
		}
	}
	return 0;
}

