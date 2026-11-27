# Fontes e licencas (consultadas em 26/11/2026)

| Fonte | Versao | Licenca | O que foi guardado |
|---|---|---|---|
| dreamcast-talk.com topico 17404 (USB4MAPLE) | 116 paginas / 1155 posts | conteudo de usuarios | `dreamcast-talk-t17404/` (posts.json, vidpid.csv) |
| USB4MAPLE firmware (wikidot) | zip 22/10/2025 | fechado (so binario) | `usb4maple-firmware/` |
| Nintendont (FIX94/Nintendont) | commit 7a18345 (18/11/2026) | GPL (cabecalho de loader/source/main.c; sem LICENSE na raiz) | `nintendont-controllerconfigs/*.ini` (43 arquivos, dados: VID/PID, offsets, mascaras). Nenhum codigo copiado. |
| Linux (torvalds/linux) | master baixado em 26/11/2026 (o `ls-remote` posterior mostrou HEAD 9437ee8b; o commit exato dos arquivos lidos nao foi registrado) | GPL-2.0 | so `linux-extracts/xpad_table.csv` (VID, PID, nome, XTYPE de xpad.c). Codigo nao guardado. Fatos usados de hid-sony.c (DS3 0xF4) e hid-nintendo.c (handshake USB). |
| Open-PS2-Loader (ps2homebrew) | commit b3a2199 | AFL-3.0 | nada guardado. Fatos lidos em modules/pademu/ds34usb.c: DS3 ativado por SET_REPORT feature 0xF4; DS3/DS4 recebem output report 0x01 (vibracao/LED). |
| FreeBSD usbdevs | main (26/11/2026) | BSD | nada guardado (3395 produtos; so consultado) |
| RiptOPL (NathanNeurotic) | - | AFL-3.0 | nada; so as descricoes do GitHub foram lidas (DualSense). Codigo NAO lido. |

Regra: dados (IDs, offsets, comandos) sao registrados com origem; nao copiar trechos de codigo GPL/AFL para o que for escrito aqui.
