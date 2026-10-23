# The maintained Linux suite builds each program once; modes share its ELF.
DASICS_UNIFIED_TEST_NAMES := \
	dasics-test-validation-smoke \
	dasics-test-csr-unified \
	dasics-test-fdicall-unified \
	dasics-test-control-flow-unified \
	dasics-test-memory-isa \
	dasics-test-memory-library \
	dasics-test-dynamic-unified \
	dasics-test-ecall-unified \
	dasics-test-maincfg-toggle-smoke \
	dasics-test-context-switch-isolation-smoke \
	dasics-test-lifecycle \
	dasics-test-utimer \
	dasics-test-amo-permission-smoke \
	dasics-test-cbo-permission-smoke \
	fdi-test-ofb \
	fdi-test-jump \
	fdi-test-cfi \
	fdi-test-maincall \
	fdi-test-finegrain \
	fdi-test-flexibility
