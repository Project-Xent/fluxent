#ifndef INSTALLER_WIZARD_H
#define INSTALLER_WIZARD_H

#include <fluxent/fluxent.h>
#include <fluxent/flux_app.h>
#include <xtk/xtk.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum WizardPage
{
	PAGE_LICENSE,
	PAGE_LOCATION,
	PAGE_COMPONENTS,
	PAGE_OPTIONS,
	PAGE_PROGRESS,
	PAGE_FINISH,
	PAGE_COUNT,
} WizardPage;

typedef enum WizardMsg
{
	MSG_NONE,
	MSG_BACK,
	MSG_NEXT,
	MSG_CANCEL,
	MSG_EXIT_RESULT,
	MSG_ACCEPT,
	MSG_DIR,
	MSG_BROWSE,
	MSG_BROWSE_PICK,
	MSG_BROWSE_RESULT,
	MSG_COMP_SELECT,
	MSG_COMP_EXPAND,
	MSG_CHANNEL_SELECT,
	MSG_CHANNEL_SUBMIT,
	MSG_UPDATE_DATE,
	MSG_CACHE,
	MSG_TICK,
	MSG_DONE,
} WizardMsg;

typedef struct WizardComp {
	char const *name;
	int         parent;
	int         mb;
	char const *dir;
	char const *stem;
	char const *ext;
} WizardComp;

enum
{
	COMP_COUNT    = 10,
	FOLDER_COUNT  = 4,
	CHANNEL_COUNT = 3,
};

extern WizardComp const  kComps [COMP_COUNT];
extern char const *const kFolders [FOLDER_COUNT];
extern char const *const kChannels [CHANNEL_COUNT];
extern char const *const kLicense;

typedef struct WizardWorker {
	FluxApp      *app;
	void         *thread;
	volatile long cancel;
	volatile long paused;
	int           count;
} WizardWorker;

typedef struct Model {
	int          page;
	bool         accepted;
	char        *dir;
	bool         browse_open;
	int          browse_sel;
	bool         checked [COMP_COUNT];
	bool         expanded [COMP_COUNT];
	int          channel_sel;
	char        *channel;
	int32_t      update_date;
	float        cache_mb;
	bool         exit_open;
	char       **files;
	int          file_count;
	int          file_done;
	char        *log;
	size_t       log_len;
	size_t       log_cap;
	bool         installing;
	bool         finished;
	bool         cancelled;
	WizardWorker worker;
} Model;

void   wizard_init(Model *m);
void   wizard_free(Model *m);
void   wizard_update(void *model, XtkMsg msg);
void   wizard_stop(FluxApp *app, void *user);
void   wizard_start(FluxApp *app, void *user);
int    wizard_required_mb(Model const *m);
bool   wizard_can_next(Model const *m);
int    wizard_flat_comp(Model const *m, int flat);

XtkEl *wizard_view(XtkUi *ui, void *model);

#endif
