#include "review_prompt.h"

bool review_should_prompt(const ReviewDecisionInput *in)
{
	if (!in || in->already_prompted)
		return false;

	if (in->cumulative_blocks >= REVIEW_BLOCK_THRESHOLD)
		return true;

	if (in->first_run_unix_seconds != 0)
	{
		uint64_t elapsed = in->now_unix_seconds - in->first_run_unix_seconds;
		if (elapsed / REVIEW_SECONDS_PER_DAY >= REVIEW_DAYS_THRESHOLD)
			return true;
	}

	return false;
}
