#!/usr/bin/env python3
"""Tonyukuk Playground API - sandboxed compile & run service."""

import os
import sys
import subprocess
import tempfile
import shutil
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import parse_qs

COMPILER = "/var/www/tonyukuktr.com/derleyici/tonyukuk-derle"
SANDBOX_USER = "tonyukuktr"
MAX_CODE_SIZE = 8192       # 8 KB max source code
COMPILE_TIMEOUT = 15       # seconds (increased for LLVM)
RUN_TIMEOUT = 5            # seconds
MAX_OUTPUT = 32768         # 32 KB max output
MAKS_WASM_BOYUT = 524288   # 512 KB max WASM binary


class PlaygroundHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path == "/run":
            self._calistir_endpoint()
        elif self.path == "/compile-wasm":
            self._wasm_derle_endpoint()
        else:
            self.send_error(404)

    # ═══════════════════════════════════════════════════════════════════════
    #                    MEVCUT: /run ENDPOINT'İ
    # ═══════════════════════════════════════════════════════════════════════

    def _calistir_endpoint(self):
        """TÜRKÇE: Mevcut çalıştır endpoint'i — native/LLVM derle ve çalıştır."""

        # Read body with size limit
        content_length = int(self.headers.get("Content-Length", 0))
        if content_length > MAX_CODE_SIZE:
            self._metin_yanit(400, "Kod boyutu cok buyuk (max 8KB)")
            return
        if content_length <= 0:
            self._metin_yanit(400, "Bos istek")
            return

        body = self.rfile.read(content_length)
        try:
            data = parse_qs(body.decode("utf-8"))
            code = data.get("kod", [""])[0]
            backend = data.get("backend", ["native"])[0]
            optimize = data.get("optimize", [""])[0]
            emit_ir = data.get("emit_ir", ["false"])[0] == "true"
        except Exception:
            self._metin_yanit(400, "Gecersiz istek")
            return

        if not code.strip():
            self._metin_yanit(400, "Bos kod")
            return

        # Validate backend option
        if backend not in ("native", "llvm"):
            backend = "native"

        # Validate optimization level
        if optimize not in ("", "-O0", "-O1", "-O2", "-O3"):
            optimize = ""

        # Create temp directory for isolation
        work_dir = tempfile.mkdtemp(prefix="play_", dir="/tmp")
        os.chmod(work_dir, 0o755)
        src_file = os.path.join(work_dir, "program.tr")
        bin_file = os.path.join(work_dir, "program")

        try:
            # Write source code
            with open(src_file, "w", encoding="utf-8") as f:
                f.write(code)

            # Build compiler command
            compile_cmd = [COMPILER, src_file, "-o", bin_file]

            if backend == "llvm":
                compile_cmd.append("--backend=llvm")
                if optimize:
                    compile_cmd.append(optimize)
                if emit_ir:
                    compile_cmd.append("--emit-llvm")

            # Compile (compiler is trusted code)
            result = subprocess.run(
                compile_cmd,
                capture_output=True, text=True,
                timeout=COMPILE_TIMEOUT,
                cwd=work_dir
            )

            if result.returncode != 0:
                output = result.stderr or result.stdout or "Derleme hatasi"
                self._metin_yanit(200, output[:MAX_OUTPUT])
                return

            # If emit_ir was requested, return IR content
            if emit_ir and backend == "llvm":
                # The compiler generates .ll file with --emit-llvm
                ll_file = bin_file + ".ll"
                if os.path.exists(ll_file):
                    with open(ll_file, "r", encoding="utf-8") as f:
                        ir_content = f.read()
                    self._metin_yanit(200, ir_content[:MAX_OUTPUT])
                    return
                else:
                    self._metin_yanit(200, "IR dosyasi olusturulamadi")
                    return

            # Make binary executable
            os.chmod(bin_file, 0o755)

            # Run as unprivileged user with timeout
            run_cmd = [
                "timeout", str(RUN_TIMEOUT),
                "sudo", "-u", SANDBOX_USER,
                "env", "-i",
                "PATH=/usr/bin:/bin",
                bin_file
            ]

            result = subprocess.run(
                run_cmd,
                capture_output=True, text=True,
                timeout=RUN_TIMEOUT + 2,
                cwd=work_dir
            )

            output = result.stdout or ""
            if result.stderr:
                output += result.stderr
            if result.returncode == 124:
                output += "\n[Zaman asimi]"

            if not output.strip():
                output = "(Cikti yok)"

            self._metin_yanit(200, output[:MAX_OUTPUT])

        except subprocess.TimeoutExpired:
            self._metin_yanit(200, "[Zaman asimi]")
        except Exception as e:
            self._metin_yanit(500, f"Sunucu hatasi: {e}")
        finally:
            # Cleanup
            shutil.rmtree(work_dir, ignore_errors=True)

    # ═══════════════════════════════════════════════════════════════════════
    #                    YENİ: /compile-wasm ENDPOINT'İ
    # ═══════════════════════════════════════════════════════════════════════

    def _wasm_derle_endpoint(self):
        """TÜRKÇE: WASM derleme endpoint'i — .wasm binary döndürür."""

        # TÜRKÇE: İstek boyutunu kontrol et
        icerik_uzunlugu = int(self.headers.get("Content-Length", 0))
        if icerik_uzunlugu > MAX_CODE_SIZE:
            self._metin_yanit(400, "Kod boyutu cok buyuk (max 8KB)")
            return
        if icerik_uzunlugu <= 0:
            self._metin_yanit(400, "Bos istek")
            return

        # TÜRKÇE: Gövdeyi oku ve kodü çıkar
        govde = self.rfile.read(icerik_uzunlugu)
        try:
            veri = parse_qs(govde.decode("utf-8"))
            kod = veri.get("kod", [""])[0]
        except Exception:
            self._metin_yanit(400, "Gecersiz istek")
            return

        if not kod.strip():
            self._metin_yanit(400, "Bos kod")
            return

        # TÜRKÇE: Geçici dizin oluştur
        calisma_dizini = tempfile.mkdtemp(prefix="wasm_", dir="/tmp")
        os.chmod(calisma_dizini, 0o755)
        kaynak_dosya = os.path.join(calisma_dizini, "program.tr")
        wasm_dosya = os.path.join(calisma_dizini, "program.wasm")

        try:
            # TÜRKÇE: Kaynak kodu dosyaya yaz
            with open(kaynak_dosya, "w", encoding="utf-8") as f:
                f.write(kod)

            # TÜRKÇE: LLVM backend ile WASM hedefine derle
            derleme_komutu = [
                COMPILER,
                "--backend=llvm",
                "-hedef", "wasm",
                kaynak_dosya,
                "-o", wasm_dosya
            ]

            sonuc = subprocess.run(
                derleme_komutu,
                capture_output=True, text=True,
                timeout=COMPILE_TIMEOUT,
                cwd=calisma_dizini
            )

            # TÜRKÇE: Derleme hatası kontrolü
            if sonuc.returncode != 0:
                hata_mesaji = sonuc.stderr or sonuc.stdout or "WASM derleme hatasi"
                self._metin_yanit(400, hata_mesaji[:MAX_OUTPUT])
                return

            # TÜRKÇE: WASM dosyası oluştu mu kontrol et
            if not os.path.exists(wasm_dosya):
                self._metin_yanit(500, "WASM dosyasi olusturulamadi")
                return

            # TÜRKÇE: Dosya boyutu kontrolü
            wasm_boyut = os.path.getsize(wasm_dosya)
            if wasm_boyut > MAKS_WASM_BOYUT:
                self._metin_yanit(400, "WASM dosyasi cok buyuk (max 512KB)")
                return

            # TÜRKÇE: WASM binary'yi oku ve gönder
            with open(wasm_dosya, "rb") as f:
                wasm_ikili = f.read()

            self._ikili_yanit(200, wasm_ikili)

        except subprocess.TimeoutExpired:
            self._metin_yanit(408, "Derleme zaman asimi")
        except Exception as e:
            self._metin_yanit(500, f"Sunucu hatasi: {e}")
        finally:
            # TÜRKÇE: Geçici dosyaları temizle
            shutil.rmtree(calisma_dizini, ignore_errors=True)

    # ═══════════════════════════════════════════════════════════════════════
    #                    YANIT YARDIMCILARI
    # ═══════════════════════════════════════════════════════════════════════

    def _metin_yanit(self, kod, metin):
        """TÜRKÇE: Düz metin yanıtı gönder."""
        self.send_response(kod)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "https://tonyukuktr.com")
        self.end_headers()
        self.wfile.write(metin.encode("utf-8"))

    def _ikili_yanit(self, kod, veri):
        """TÜRKÇE: Binary (WASM) yanıtı gönder."""
        self.send_response(kod)
        self.send_header("Content-Type", "application/wasm")
        self.send_header("Content-Length", str(len(veri)))
        self.send_header("Access-Control-Allow-Origin", "https://tonyukuktr.com")
        self.send_header("Access-Control-Expose-Headers", "Content-Length")
        self.end_headers()
        self.wfile.write(veri)

    def do_GET(self):
        if self.path == "/saglik":
            self._metin_yanit(200, "calisiyor")
        else:
            self.send_error(405)

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "https://tonyukuktr.com")
        self.send_header("Access-Control-Allow-Methods", "POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def log_message(self, format, *args):
        try:
            msg = format % args
        except Exception:
            msg = str(args)
        sys.stderr.write(f"[playground] {msg}\n")


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8081
    server = HTTPServer(("127.0.0.1", port), PlaygroundHandler)
    print(f"Playground API listening on 127.0.0.1:{port}", file=sys.stderr)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...", file=sys.stderr)
        server.shutdown()
