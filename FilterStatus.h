#ifndef FILTERSTATUS_H
#define FILTERSTATUS_H

struct FilterStatus {
	bool *cancel = nullptr;
	float *progress = nullptr;
	FilterStatus(bool *cancel, float *progress)
		: cancel(cancel)
		, progress(progress)
	{
	}
};

#endif // FILTERSTATUS_H
