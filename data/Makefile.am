configdir = $(datadir)/@PACKAGE@
dist_config_DATA = motoko.conf.default plugin.conf.default wrequest.db.default

install-data-hook:
	mkdir -p $(HOME)/.@PACKAGE@
	mkdir -p $(HOME)/.@PACKAGE@/profiles
	mkdir -p $(HOME)/.@PACKAGE@/profiles/default
	mkdir -p $(HOME)/.@PACKAGE@/warden
	mkdir -p $(HOME)/.@PACKAGE@/warden/modules
	install -c -m 600 --backup motoko.conf.default $(HOME)/.@PACKAGE@/profiles/default/motoko.conf
	install -c -m 600 --backup plugin.conf.default $(HOME)/.@PACKAGE@/profiles/default/plugin.conf
	install -c -m 600 --backup wrequest.db.default $(HOME)/.@PACKAGE@/warden/wrequest.db
