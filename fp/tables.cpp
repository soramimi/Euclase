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
	fp16_gamma_table();
	return 0;
}
