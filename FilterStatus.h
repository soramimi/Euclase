#ifndef FILTERSTATUS_H
#define FILTERSTATUS_H

#include <atomic>

struct FilterStatus {
	std::atomic_bool *cancel = nullptr;
	std::atomic<float> *progress = nullptr;
	FilterStatus(std::atomic_bool *cancel, std::atomic<float> *progress)
		: cancel(cancel)
		, progress(progress)
	{
	}
};

#endif // FILTERSTATUS_H
