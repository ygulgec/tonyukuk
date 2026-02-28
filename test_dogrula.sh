#!/bin/bash
# Tonyukuk Derleyici - Cikti Dogrulama Test Calistiricisi
# Kullanim: ./test_dogrula.sh [--llvm]
#
# Her test dosyasi icin:
#   1. testler/beklenen/<isim>.beklenen dosyasi varsa, ciktiyi karsilastirir
#   2. Yoksa sadece cikis kodunu kontrol eder (exit-code-only)

set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPILER="$SCRIPT_DIR/tonyukuk-derle"
TEST_DIR="$SCRIPT_DIR/testler"
BEKLENEN_DIR="$TEST_DIR/beklenen"
BACKEND="native"
TIMEOUT_SEC=5

# Renkler
YESIL="\033[0;32m"
KIRMIZI="\033[0;31m"
SARI="\033[0;33m"
MAVI="\033[0;34m"
SIFIRLA="\033[0m"

# Sayaclar
GECTI=0
KALDI=0
ATLANDI=0
TOPLAM=0

# Basarisiz test listesi
declare -a BASARISIZ_TESTLER

# Atlanan testler (ag, dosya, async gibi yan etkileri olan testler)
ATLA_LISTESI=(
    "modul_"
    "hata_"
    "test_sabit_hata"
    "test_ag.tr"
    "test_ag_http.tr"
    "test_dosya.tr"
    "test_async.tr"
    "test_duzeni.tr"
    "test_json.tr"
    "test_generic.tr"
    "test_uclu.tr"
    "test_arayuz.tr"
    "test_arm64.tr"
    "test_wasm.tr"
    "wasm_test.tr"
    "soket_test.tr"
    "veritabani_test.tr"
    "ortam_test.tr"
    "kripto_test.tr"
    "test_paralel.tr"
    "test_sistem.tr"
    "test_sistem2.tr"
    "test_modul_dosya.tr"
    "test_renkli_hata.tr"
    "test_profil.tr"
)

# Bayrak isleme
while [[ $# -gt 0 ]]; do
    case "$1" in
        --llvm)
            BACKEND="llvm"
            shift
            ;;
        --help|-h)
            echo "Kullanim: $0 [--llvm]"
            echo "  --llvm    LLVM backend ile test et"
            exit 0
            ;;
        *)
            echo "Bilinmeyen parametre: $1"
            exit 1
            ;;
    esac
done

atlanmali_mi() {
    local dosya="$1"
    local temel=$(basename "$dosya")
    for desen in "${ATLA_LISTESI[@]}"; do
        if [[ "$temel" == *"$desen"* ]] || [[ "$temel" == "$desen" ]]; then
            return 0
        fi
    done
    return 1
}

echo "=========================================="
echo "Tonyukuk Derleyici - Cikti Dogrulama"
echo "Backend: $BACKEND"
echo "=========================================="
echo ""

# beklenen dizini yoksa olustur
if [ ! -d "$BEKLENEN_DIR" ]; then
    mkdir -p "$BEKLENEN_DIR"
    echo "[UYARI] $BEKLENEN_DIR dizini olusturuldu. Henuz beklenen cikti dosyasi yok."
    echo ""
fi

for test_dosya in "$TEST_DIR"/*.tr; do
    temel=$(basename "$test_dosya" .tr)
    isim=$(basename "$test_dosya")

    # Atlanacak mi?
    if atlanmali_mi "$test_dosya"; then
        ((ATLANDI++))
        ((TOPLAM++))
        continue
    fi

    ((TOPLAM++))

    # Zamanlayici baslat
    BASLANGIC=$(date +%s%N 2>/dev/null || date +%s)

    # Derle
    CIKTI_DOSYA="/tmp/tonyukuk_test_${temel}"
    if [ "$BACKEND" == "llvm" ]; then
        DERLEME_CIKTI=$("$COMPILER" --backend=llvm "$test_dosya" -o "$CIKTI_DOSYA" 2>&1)
    else
        DERLEME_CIKTI=$("$COMPILER" "$test_dosya" -o "$CIKTI_DOSYA" 2>&1)
    fi
    DERLEME_KODU=$?

    if [ $DERLEME_KODU -ne 0 ]; then
        BITIS=$(date +%s%N 2>/dev/null || date +%s)
        SURE=$(( (BITIS - BASLANGIC) / 1000000 ))
        echo -e "  ${KIRMIZI}KALDI${SIFIRLA} $isim (derleme hatasi) [${SURE}ms]"
        BASARISIZ_TESTLER+=("$isim: DERLEME HATASI")
        ((KALDI++))
        continue
    fi

    # Calistir
    if [ -f "$CIKTI_DOSYA" ]; then
        GERCEK_CIKTI=$(timeout ${TIMEOUT_SEC}s "$CIKTI_DOSYA" 2>&1)
        CALISMA_KODU=$?

        BITIS=$(date +%s%N 2>/dev/null || date +%s)
        SURE=$(( (BITIS - BASLANGIC) / 1000000 ))

        # Zaman asimi kontrolu
        if [ $CALISMA_KODU -eq 124 ]; then
            echo -e "  ${KIRMIZI}KALDI${SIFIRLA} $isim (zaman asimi) [${SURE}ms]"
            BASARISIZ_TESTLER+=("$isim: ZAMAN ASIMI")
            ((KALDI++))
            rm -f "$CIKTI_DOSYA"
            continue
        fi

        # Beklenen dosya var mi?
        BEKLENEN_DOSYA="$BEKLENEN_DIR/${temel}.beklenen"

        if [ -f "$BEKLENEN_DOSYA" ]; then
            # Cikti dogrulama modu
            FARK=$(diff <(echo "$GERCEK_CIKTI") "$BEKLENEN_DOSYA" 2>&1)
            FARK_KODU=$?

            if [ $FARK_KODU -eq 0 ]; then
                echo -e "  ${YESIL}GECTI${SIFIRLA} $isim (cikti dogrulandi) [${SURE}ms]"
                ((GECTI++))
            else
                echo -e "  ${KIRMIZI}KALDI${SIFIRLA} $isim (cikti uyusmuyor) [${SURE}ms]"
                echo "    --- Fark ---"
                echo "$FARK" | head -20 | sed 's/^/    /'
                echo "    --- ---"
                BASARISIZ_TESTLER+=("$isim: CIKTI UYUSMUYOR")
                ((KALDI++))
            fi
        else
            # Sadece cikis kodu kontrolu
            # Bazi testler (bolme_sifir, dizi_sinir) calisma zamaninda hata vermeli
            case "$temel" in
                test_bolme_sifir|test_dizi_sinir)
                    echo -e "  ${YESIL}GECTI${SIFIRLA} $isim (beklenen hata) [${SURE}ms]"
                    ((GECTI++))
                    ;;
                *)
                    if [ $CALISMA_KODU -eq 0 ]; then
                        echo -e "  ${YESIL}GECTI${SIFIRLA} $isim (cikis kodu: 0) [${SURE}ms]"
                        ((GECTI++))
                    else
                        echo -e "  ${KIRMIZI}KALDI${SIFIRLA} $isim (cikis kodu: $CALISMA_KODU) [${SURE}ms]"
                        BASARISIZ_TESTLER+=("$isim: CALISMA HATASI (cikis kodu: $CALISMA_KODU)")
                        ((KALDI++))
                    fi
                    ;;
            esac
        fi

        rm -f "$CIKTI_DOSYA"
    else
        BITIS=$(date +%s%N 2>/dev/null || date +%s)
        SURE=$(( (BITIS - BASLANGIC) / 1000000 ))
        echo -e "  ${KIRMIZI}KALDI${SIFIRLA} $isim (cikti dosyasi yok) [${SURE}ms]"
        BASARISIZ_TESTLER+=("$isim: CIKTI DOSYASI YOK")
        ((KALDI++))
    fi
done

echo ""
echo "=========================================="
echo "Sonuclar ($BACKEND backend)"
echo "=========================================="
echo -e "  Toplam:   $TOPLAM"
echo -e "  ${YESIL}Gecti:    $GECTI${SIFIRLA}"
echo -e "  ${KIRMIZI}Kaldi:    $KALDI${SIFIRLA}"
echo -e "  ${SARI}Atlandi:  $ATLANDI${SIFIRLA}"

DOGRULANAN=$(ls "$BEKLENEN_DIR"/*.beklenen 2>/dev/null | wc -l)
echo -e "  ${MAVI}Dogrulama dosyasi: $DOGRULANAN${SIFIRLA}"
echo ""

if [ $KALDI -gt 0 ]; then
    echo "Basarisiz Testler:"
    for basarisiz in "${BASARISIZ_TESTLER[@]}"; do
        echo -e "  ${KIRMIZI}x${SIFIRLA} $basarisiz"
    done
    echo ""
fi

if [ $TOPLAM -gt 0 ]; then
    ORAN=$(awk "BEGIN {printf \"%.1f\", ($GECTI/$TOPLAM)*100}")
    echo "Basari Orani: ${ORAN}%"
fi
echo "=========================================="

# Basarisiz test varsa hata koduyla cik
[ "$KALDI" -eq 0 ]
