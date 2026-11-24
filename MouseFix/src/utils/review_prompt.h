#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Thresholds for the review prompt. A prompt fires once when the app has
   filtered 200 accidental clicks, or once the first run is 7 days old,
   whichever comes first. */
#define REVIEW_BLOCK_THRESHOLD 200u
#define REVIEW_DAYS_THRESHOLD 7u
#define REVIEW_SECONDS_PER_DAY 86400u

typedef struct
{
	uint64_t cumulative_blocks;    /* filtered clicks, never reset by the user */
	uint64_t first_run_unix_seconds; /* 0 when no first-run time is recorded */
	bool already_prompted;
	uint64_t now_unix_seconds;
} ReviewDecisionInput;

/* True when the review prompt should be shown now. Pure, so it is unit-testable. */
bool review_should_prompt(const ReviewDecisionInput *in);
