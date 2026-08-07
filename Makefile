# Single entry point for every native test suite (no hardware needed).
# CI runs the same three suites as separate steps for clearer failure attribution.
.PHONY: test
test:
	$(MAKE) -C test/native
	node test/test_ui.js
	python3 -m pytest test/ -q
