#include <sys/param.h>
#include <sys/types.h>

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <libutil.h>

#include <pkg.h>

#include "upgrade.h"
#include "utils.h"

void
usage_upgrade(void)
{
	fprintf(stderr, "usage pkg upgrade [-y]\n");
	fprintf(stderr, "For more information see 'pkg help upgrade'.\n");
}

int
exec_upgrade(int argc, char **argv)
{
	struct pkgdb *db = NULL;
	struct pkgdb_it *it = NULL;
	struct pkg *pkg = NULL;
	struct pkg_jobs *jobs = NULL;
	int retcode = 1;
	int64_t oldsize = 0, newsize = 0;
	int64_t dlsize = 0;
	char size[7];
	int ch, yes = 0;

	if (geteuid() != 0) {
		warnx("upgrading can only be done as root");
		return (EX_NOPERM);
	}

	while ((ch = getopt(argc, argv, "y")) != -1) {
		switch (ch) {
			case 'y':
				yes = 1;
				break;
			default:
				break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0) {
		usage_upgrade();
		return (EX_USAGE);
	}

	if (pkgdb_open(&db, PKGDB_REMOTE) != EPKG_OK) {
		return (EX_IOERR);
	}

	if (pkg_jobs_new(&jobs, PKG_JOBS_INSTALL, db) != EPKG_OK) {
		goto cleanup;
	}

	if ((it = pkgdb_query_upgrades(db)) == NULL) {
		goto cleanup;
	}

	while (pkgdb_it_next(it, &pkg, PKG_LOAD_BASIC|PKG_LOAD_DEPS) == EPKG_OK) {
		pkg_jobs_add(jobs, pkg);
		pkg = NULL;
	}

	if (pkg_jobs_is_empty(jobs)) {
		printf("Nothing to do\n");
		retcode = 0;
		goto cleanup;
	}

	printf("The following packages will be upgraded: \n");
	pkg = NULL;
	while (pkg_jobs(jobs, &pkg) == EPKG_OK) {
		dlsize += pkg_new_pkgsize(pkg);
		if (pkg_get(pkg, PKG_NEWVERSION) != NULL) {
			printf("\tUpgrading %s: %s -> %s\n", pkg_get(pkg, PKG_NAME), pkg_get(pkg, PKG_VERSION), pkg_get(pkg,PKG_NEWVERSION));
			oldsize += pkg_flatsize(pkg);
			newsize += pkg_new_flatsize(pkg);
		} else {
			newsize += pkg_flatsize(pkg);
			printf("\tInstalling %s: %s\n", pkg_get(pkg, PKG_NAME), pkg_get(pkg, PKG_VERSION));
		}
	}

	if (oldsize > newsize) {
		newsize *= -1;
		humanize_number(size, sizeof(size), oldsize - newsize, "B", HN_AUTOSCALE, 0);
		printf("\nthe upgrade will save %s\n", size);
	} else {
		humanize_number(size, sizeof(size), newsize - oldsize, "B", HN_AUTOSCALE, 0);
		printf("\nthe upgrade will require %s more space\n", size);
	}
	humanize_number(size, sizeof(size), dlsize, "B", HN_AUTOSCALE, 0);
	printf("%s to be download_ed\n", size);

	if (yes == 0)
		yes = query_yesno("\nProceed with upgrading packages [y/N]: ");

	if (yes == 1)
		if (pkg_jobs_apply(jobs, 0) != EPKG_OK)
			goto cleanup;

	retcode = 0;

	cleanup:
	pkgdb_it_free(it);
	pkg_jobs_free(jobs);
	pkgdb_close(db);

	return (retcode);
}
