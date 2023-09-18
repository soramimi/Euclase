#ifndef FILTERSTATUS_H
#define FILTERSTATUS_H

class FilterContext;

struct FilterStatus {
	FilterStatus(FilterContext *context);
	bool *cancel = nullptr;
	float *progress = nullptr;
};



#endif // FILTERSTATUS_H
