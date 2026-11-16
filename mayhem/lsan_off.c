/* Build-time LeakSanitizer off-switch (SPEC §6.2 item 15); ASan and UBSan stay active. */
int __lsan_is_turned_off(void) { return 1; }
