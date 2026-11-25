#include "fluxent/flux_toast.h"

#include <cwinrt/Windows.Data.Xml.Dom.h>
#include <cwinrt/Windows.UI.Notifications.h>
#include <cwinrt/factory.h>
#include <roapi.h>

static HRESULT toast_create(wchar_t const *xml, WUINO_ToastNotification **out) {
	WDAXMDO_XmlDocument             *document = NULL;
	WUINO_IToastNotificationFactory *factory  = NULL;
	cwinrt_hstring                   source   = NULL;
	HRESULT                          hr       = cwinrt_hstring_from(xml, &source);
	if (SUCCEEDED(hr)) hr = wdaxmdo_xml_document_new(&document);
	if (SUCCEEDED(hr)) hr = wdaxmdo_xml_document_load_xml(document, source);
	if (SUCCEEDED(hr))
		hr = cwinrt_factory_get_statics(
		  L"Windows.UI.Notifications.ToastNotification", &CWINRT_IID_WUINO_IToastNotificationFactory,
		  ( void ** ) &factory
		);
	if (SUCCEEDED(hr)) hr = wuino_toast_notification_factory_create_toast_notification(factory, document, out);
	if (factory) IUnknown_Release(( IUnknown * ) factory);
	if (document) IUnknown_Release(( IUnknown * ) document);
	cwinrt_hstring_free(source);
	return hr;
}

static HRESULT toast_notifier(wchar_t const *app_id, WUINO_ToastNotifier **out) {
	WUINO_IToastNotificationManagerStatics *manager = NULL;
	HRESULT                                 hr      = cwinrt_factory_get_statics(
	  L"Windows.UI.Notifications.ToastNotificationManager", &CWINRT_IID_WUINO_IToastNotificationManagerStatics,
	  ( void ** ) &manager
	);
	if (FAILED(hr)) return hr;
	cwinrt_hstring id = NULL;
	if (app_id) hr = cwinrt_hstring_from(app_id, &id);
	if (SUCCEEDED(hr))
		hr = app_id ? wuino_toast_notification_manager_statics_create_toast_notifier_str_p(manager, id, out)
		            : wuino_toast_notification_manager_statics_create_toast_notifier(manager, out);
	cwinrt_hstring_free(id);
	IUnknown_Release(( IUnknown * ) manager);
	return hr;
}

HRESULT flux_toast_show(wchar_t const *app_id, wchar_t const *xml) {
	if (!xml || !*xml || (app_id && !*app_id)) return E_INVALIDARG;
	HRESULT initialized = RoInitialize(RO_INIT_SINGLETHREADED);
	if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return initialized;
	WUINO_ToastNotification *notification = NULL;
	WUINO_ToastNotifier     *notifier     = NULL;
	HRESULT                  hr           = toast_create(xml, &notification);
	if (SUCCEEDED(hr)) hr = toast_notifier(app_id, &notifier);
	if (SUCCEEDED(hr)) hr = wuino_toast_notifier_show(notifier, notification);
	if (notifier) IUnknown_Release(( IUnknown * ) notifier);
	if (notification) IUnknown_Release(( IUnknown * ) notification);
	if (SUCCEEDED(initialized)) RoUninitialize();
	return hr;
}
