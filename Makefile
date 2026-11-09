# Single entry point for every native test suite (no hardware needed).
# CI runs the same three suites as separate steps for clearer failure attribution.
.PHONY: test mutants
test:
	$(MAKE) -C test/native
	node test/test_ui.js
	node --test 'test/ui/*.test.js'
	python3 -m pytest test/ -q

# Re-introduces every bug already fixed in the UI poll/slider/OTA/AI/preset paths
# plus the hunted regressions, and requires test/ui/*.test.js to fail on each (~2.5 min; run in CI).
mutants:
	node test/ui_mutants.js
