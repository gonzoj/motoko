if WITH_LIBWARDENC
wardencdata = wrequest.db.default
endif

dist_config_DATA = motoko.conf.default plugin.conf.default $(wardencdata)

install-data-hook:
	mkdir -p $(HOME)/.$(PACKAGE)
	mkdir -p $(HOME)/.$(PACKAGE)/profiles
	mkdir -p $(HOME)/.$(PACKAGE)/profiles/default
if WITH_LIBWARDENC
	mkdir -p $(HOME)/.$(PACKAGE)/warden
	mkdir -p $(HOME)/.$(PACKAGE)/warden/modules
endif
	install -c -m 600 -b motoko.conf.default $(HOME)/.$(PACKAGE)/profiles/default/motoko.conf
	install -c -m 600 -b plugin.conf.default $(HOME)/.$(PACKAGE)/profiles/default/plugin.conf
if WITH_LIBWARDENC
	install -c -m 600 -b wrequest.db.default $(HOME)/.$(PACKAGE)/warden/wrequest.db
endif
