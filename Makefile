CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O2 -Isrc
LDFLAGS =

# GTK3 ve WebKit2GTK ayarları (GUI modülleri için)
GTK_MEVCUT = $(shell pkg-config --exists gtk+-3.0 2>/dev/null && echo "1" || echo "0")
WEBKIT_MEVCUT = $(shell pkg-config --exists webkit2gtk-4.1 2>/dev/null && echo "1" || echo "0")
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null || echo "")
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0 2>/dev/null || echo "")
WEBKIT_CFLAGS = $(shell pkg-config --cflags webkit2gtk-4.1 2>/dev/null || echo "")
WEBKIT_LIBS = $(shell pkg-config --libs webkit2gtk-4.1 2>/dev/null || echo "")

# LLVM ayarları (LLVM 17)
LLVM_CONFIG = llvm-config-17
LLVM_CFLAGS = $(shell $(LLVM_CONFIG) --cflags 2>/dev/null || echo "")
# Not: llvm-config symlink sorunları nedeniyle doğrudan kütüphane yolunu belirtiyoruz
LLVM_LDFLAGS = -L/usr/lib/x86_64-linux-gnu -lLLVM-17
LLVM_MEVCUT = $(shell which $(LLVM_CONFIG) >/dev/null 2>&1 && echo "1" || echo "0")

SRCS = src/ana.c src/sozcuk.c src/cozumleyici.c src/agac.c \
       src/anlam.c src/uretici.c src/uretici_arm64.c src/uretici_wasm.c \
       src/uretici_avr.c src/uretici_xtensa.c src/uretici_arm_m0.c \
       src/uretici_vm.c \
       src/utf8.c src/hata.c \
       src/tablo.c src/bellek.c src/metin.c \
       src/optimize.c src/kaynak_harita.c \
       src/modul.c src/modul_kayit_gen.c

# LLVM backend (opsiyonel)
ifeq ($(LLVM_MEVCUT),1)
SRCS += src/llvm_uretici.c src/wasm_kopru.c
CFLAGS += -DLLVM_BACKEND_MEVCUT $(LLVM_CFLAGS)
LDFLAGS += $(LLVM_LDFLAGS)
endif

# Modül tanım dosyaları (derleme zamanı, derleyiciye bağlanır)
MODUL_TANIM_SRCS = $(wildcard stdlib/*_tanim.c)
MODUL_TANIM_OBJS = $(MODUL_TANIM_SRCS:.c=.o)

OBJS = $(SRCS:.c=.o) $(MODUL_TANIM_OBJS)
TARGET = tonyukuk-derle

# Çalışma zamanı kütüphanesi
RUNTIME_SRC = src/calismazamani.c
# Modül runtime dosyaları (libtr.a'ya bağlanır)
MODUL_CZ_SRCS = $(wildcard stdlib/*_cz.c)
MODUL_CZ_OBJS = $(MODUL_CZ_SRCS:.c=.o)
RUNTIME_OBJ = src/calismazamani.o
RUNTIME_LIB = libtr.a

# Biçimleyici (Formatter)
BICIMLE_OBJS = src/bicimle.o src/sozcuk.o src/utf8.o src/hata.o

# Denetleyici (Linter)
LINT_OBJS = src/denetle.o src/sozcuk.o src/cozumleyici.o src/agac.o src/anlam.o src/tablo.o src/bellek.o src/metin.o src/utf8.o src/hata.o

PROD_CFLAGS = -Wall -Wextra -std=c11 -O2 -Isrc -DNDEBUG \
              -fstack-protector-strong -D_FORTIFY_SOURCE=2 \
              -Wformat -Wformat-security

# Windows cross-compile ayarları
WIN_CC = x86_64-w64-mingw32-gcc
WIN_CFLAGS = -std=c11 -O2 -Wall -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN
WIN_AR = x86_64-w64-mingw32-ar
WIN_RUNTIME_LIB = libtr_win.a

# Windows modül kaynak dosyaları
WIN_GUI_SRCS = stdlib/win/pencere_win_cz.c stdlib/win/arayuz_win_cz.c stdlib/win/webgorunum_win_cz.c
WIN_GUI_OBJS = $(WIN_GUI_SRCS:.c=.o)
WIN_RUNTIME_SRC = stdlib/win/calismazamani_win.c
WIN_RUNTIME_OBJ = stdlib/win/calismazamani_win.o

# Windows'a uyumlu modüller (GUI olmayanlar)
# Sadece cross-compile edilebilen modüller (POSIX bağımlı olanları çıkar)
WIN_COMPAT_SRCS = $(filter-out stdlib/pencere_cz.c stdlib/arayuz_cz.c stdlib/webgorunum_cz.c stdlib/soket_cz.c stdlib/paralel_cz.c stdlib/donanim_cz.c stdlib/ag_cz.c stdlib/veritabani_cz.c stdlib/kripto_cz.c stdlib/ortam_cz.c stdlib/dosya_cz.c stdlib/tarih_cz.c stdlib/zaman_cz.c stdlib/duzeni_cz.c,$(MODUL_CZ_SRCS))
WIN_COMPAT_OBJS = $(patsubst stdlib/%.c,stdlib/win/compat_%.o,$(WIN_COMPAT_SRCS))

.PHONY: all clean test production llvm-info win test-dogrula test-dogrula-llvm

all: $(TARGET) $(RUNTIME_LIB) trsm
	@if [ "$(LLVM_MEVCUT)" = "1" ]; then \
		echo "  LLVM backend aktif ($(shell $(LLVM_CONFIG) --version))"; \
	else \
		echo "  LLVM backend devre dışı (llvm-config-17 bulunamadı)"; \
	fi

# LLVM bilgisi göster
llvm-info:
	@echo "LLVM Durumu:"
	@if [ "$(LLVM_MEVCUT)" = "1" ]; then \
		echo "  Versiyon: $(shell $(LLVM_CONFIG) --version)"; \
		echo "  CFLAGS: $(LLVM_CFLAGS)"; \
		echo "  LDFLAGS: $(LLVM_LDFLAGS)"; \
	else \
		echo "  LLVM bulunamadı. Kurmak için: sudo apt install llvm-17-dev"; \
	fi

bicimle: $(BICIMLE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

denetle: $(LINT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

ton: src/paket.o
	$(CC) $(CFLAGS) -o $@ $^

trdoc: src/belgeleme.o
	$(CC) $(CFLAGS) -o $@ $^

tonyukuk-lsp: src/lsp.o src/sozcuk.o src/cozumleyici.o src/agac.o src/anlam.o src/tablo.o src/bellek.o src/metin.o src/utf8.o src/hata.o src/modul.o src/lsp_modul_stub.o
	$(CC) $(CFLAGS) -o $@ $^

# Hata Ayıklayıcı (Debugger)
tonyukuk-ha: src/hataayikla.o
	$(CC) $(CFLAGS) -o $@ $^

# Tonyukuk Sanal Makinesi (bytecode yorumlayıcı)
trsm: src/trsm.c src/vm.h
	$(CC) -std=c11 -Wall -Wextra -g -O2 -o trsm src/trsm.c

playground-api: web/playground_api.c
	$(CC) -std=c11 -O2 -Wall -Wextra -o web/playground-api web/playground_api.c

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Modül kayıt dosyasını _tanim.c dosyalarından otomatik üret
MODUL_ISIMLERI = $(patsubst stdlib/%_tanim.c,%,$(MODUL_TANIM_SRCS))

src/modul_kayit_gen.c: $(MODUL_TANIM_SRCS) Makefile
	@echo "/* Otomatik üretilmiş modül kayıt dosyası — elle düzenlemeyin */" > $@
	@echo "#include \"modul.h\"" >> $@
	@echo "" >> $@
	@for m in $(MODUL_ISIMLERI); do echo "extern const ModülTanım $${m}_modul;"; done >> $@
	@echo "" >> $@
	@echo "const ModülTanım *tum_moduller[] = {" >> $@
	@for m in $(MODUL_ISIMLERI); do echo "    &$${m}_modul,"; done >> $@
	@echo "};" >> $@
	@echo "" >> $@
	@echo "int modul_sayisi = sizeof(tum_moduller) / sizeof(tum_moduller[0]);" >> $@
	@echo "  Otomatik üretildi: src/modul_kayit_gen.c ($(words $(MODUL_ISIMLERI)) modül)"

$(RUNTIME_LIB): $(RUNTIME_OBJ) $(MODUL_CZ_OBJS)
	ar rcs $@ $^

$(RUNTIME_OBJ): $(RUNTIME_SRC)
	$(CC) -std=c11 -g -O2 -c -o $@ $< -lm

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

stdlib/%.o: stdlib/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# GTK/WebKit modülleri özel derleme kuralları
stdlib/pencere_tanim.o: stdlib/pencere_tanim.c
	$(CC) $(CFLAGS) -c -o $@ $<

stdlib/pencere_cz.o: stdlib/pencere_cz.c
	$(CC) -std=c11 -g -O2 $(GTK_CFLAGS) -c -o $@ $<

stdlib/arayuz_tanim.o: stdlib/arayuz_tanim.c
	$(CC) $(CFLAGS) -c -o $@ $<

stdlib/arayuz_cz.o: stdlib/arayuz_cz.c
	$(CC) -std=c11 -g -O2 $(GTK_CFLAGS) -c -o $@ $<

stdlib/webgorunum_tanim.o: stdlib/webgorunum_tanim.c
	$(CC) $(CFLAGS) -c -o $@ $<

stdlib/webgorunum_cz.o: stdlib/webgorunum_cz.c
	$(CC) -std=c11 -g -O2 $(GTK_CFLAGS) $(WEBKIT_CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(RUNTIME_OBJ) $(MODUL_CZ_OBJS) $(TARGET) $(RUNTIME_LIB)
	rm -f src/bicimle.o src/denetle.o src/paket.o src/belgeleme.o src/lsp.o src/hataayikla.o src/llvm_uretici.o
	rm -f bicimle denetle ton trdoc tonyukuk-lsp tonyukuk-ha trsm
	rm -f testler/*.s testler/*.o testler/*.wat testler/*.ll testler/*.bc

test: $(TARGET)
	@echo "=== Merhaba Dünya testi ==="
	./$(TARGET) testler/merhaba.tr -o testler/merhaba
	./testler/merhaba
	@echo ""
	@echo "=== Değişken testi ==="
	./$(TARGET) testler/degiskenler.tr -o testler/degiskenler
	./testler/degiskenler
	@echo ""
	@echo "=== Kontrol akışı testi ==="
	./$(TARGET) testler/kosullar.tr -o testler/kosullar
	./testler/kosullar
	@echo ""
	@echo "=== İşlev testi ==="
	./$(TARGET) testler/islevler.tr -o testler/islevler
	./testler/islevler
	@echo ""
	@echo "=== Döngü testi ==="
	./$(TARGET) testler/donguler.tr -o testler/donguler
	./testler/donguler
	@echo ""
	@echo "=== Test çerçevesi testi ==="
	./$(TARGET) --test testler/test_cerceve.tr -o testler/test_cerceve
	./testler/test_cerceve
	@echo "=== Tüm testler başarılı ==="

wasm-test: $(TARGET)
	@echo "=== WASM Binary Testi: merhaba ==="
	./$(TARGET) --backend=llvm -hedef wasm testler/merhaba.tr -o testler/merhaba.wasm
	wasm2wat testler/merhaba.wasm -o testler/merhaba_kontrol.wat
	@echo "✓ merhaba.tr WASM testi başarılı"
	@rm -f testler/merhaba.wasm testler/merhaba_kontrol.wat
	@echo "=== WASM Binary Testi: wasm_test (döngü+fibonacci+faktoriyel) ==="
	./$(TARGET) --backend=llvm -hedef wasm testler/wasm_test.tr -o testler/wasm_test.wasm
	wasm2wat testler/wasm_test.wasm -o testler/wasm_test_kontrol.wat
	@echo "✓ wasm_test.tr WASM testi başarılı"
	@rm -f testler/wasm_test.wasm testler/wasm_test_kontrol.wat
	@echo "=== Tüm WASM testleri başarılı ==="

production: clean
	$(MAKE) CFLAGS="$(PROD_CFLAGS)" all
	strip $(TARGET)

# ========== Windows Cross-Compile ==========

win: $(TARGET) $(WIN_RUNTIME_LIB)
	@echo "  Windows runtime kütüphanesi oluşturuldu: $(WIN_RUNTIME_LIB)"

$(WIN_RUNTIME_LIB): $(WIN_RUNTIME_OBJ) $(WIN_GUI_OBJS) $(WIN_COMPAT_OBJS)
	$(WIN_AR) rcs $@ $^
	@echo "  libtr_win.a oluşturuldu ($(words $^) nesne dosyası)"

$(WIN_RUNTIME_OBJ): $(WIN_RUNTIME_SRC)
	$(WIN_CC) $(WIN_CFLAGS) -c -o $@ $<

stdlib/win/pencere_win_cz.o: stdlib/win/pencere_win_cz.c stdlib/win/win_ortak.h
	$(WIN_CC) $(WIN_CFLAGS) -Istdlib -c -o $@ $<

stdlib/win/arayuz_win_cz.o: stdlib/win/arayuz_win_cz.c stdlib/win/win_ortak.h
	$(WIN_CC) $(WIN_CFLAGS) -Istdlib -c -o $@ $<

stdlib/win/webgorunum_win_cz.o: stdlib/win/webgorunum_win_cz.c stdlib/win/win_ortak.h
	$(WIN_CC) $(WIN_CFLAGS) -Istdlib -c -o $@ $<

# Windows uyumlu modülleri MinGW ile derle
stdlib/win/compat_%.o: stdlib/%.c
	@mkdir -p stdlib/win
	$(WIN_CC) $(WIN_CFLAGS) -Istdlib -c -o $@ $<

win-clean:
	rm -f stdlib/win/*.o $(WIN_RUNTIME_LIB)

# ========== Cikti Dogrulama Testi ==========

test-dogrula: $(TARGET)
	@bash test_dogrula.sh

test-dogrula-llvm: $(TARGET)
	@bash test_dogrula.sh --llvm
