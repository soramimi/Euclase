
#include "FilterDialog.h"
#include "FilterStatus.h"

FilterStatus::FilterStatus(FilterContext *context)
{
	cancel = context->cancel_ptr();
	progress = context->progress_ptr();
}
