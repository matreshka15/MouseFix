#include "store_license.h"

#include <thread>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Services.Store.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Services::Store;

static StoreLicenseState query_impl(int *days_out)
{
	*days_out = 0;

	StoreContext context = StoreContext::GetDefault();
	StoreAppLicense license = context.GetAppLicenseAsync().get();

	StoreLicenseState state = store_license_map(license.IsActive(), license.IsTrial());

	if (state == STORE_LICENSE_TRIAL)
	{
		TimeSpan remaining = license.TrialTimeRemaining();
		/* TimeSpan::count() is 100ns ticks. Round up so a trial that is nearly
		   over still shows "1 day" instead of "0 days". */
		int64_t seconds = remaining.count() / 10000000;
		int64_t days = seconds / 86400;
		if (seconds % 86400 != 0)
			++days;
		if (days < 0)
			days = 0;
		*days_out = (int)days;
	}

	return state;
}

void store_license_query(StoreLicenseInfo *out)
{
	if (!out)
		return;

	out->state = STORE_LICENSE_UNKNOWN;
	out->days_remaining = 0;

	try
	{
		init_apartment(apartment_type::multi_threaded);
		out->state = query_impl(&out->days_remaining);
	}
	catch (...)
	{
		out->state = STORE_LICENSE_UNKNOWN;
		out->days_remaining = 0;
	}
}

bool store_license_request_rate_and_review(void)
{
	try
	{
		init_apartment(apartment_type::multi_threaded);
		StoreContext context = StoreContext::GetDefault();

		/* RequestRateAndReviewAppAsync lives on IStoreContext4 (Store contract
		   4.0, Windows 10 2004). On an older OS this call throws, so we catch
		   it and the caller falls back to the review URL. Starting it here also
		   proves the context is usable before we detach. */
		auto operation = context.RequestRateAndReviewAppAsync();

		/* The dialog stays up until the user answers; block a worker thread so
		   our UI thread never freezes. */
		std::thread([operation]() {
			try
			{
				init_apartment(apartment_type::multi_threaded);
				operation.get();
			}
			catch (...)
			{
			}
		}).detach();

		return true;
	}
	catch (...)
	{
		return false;
	}
}
