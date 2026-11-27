# Guia de leitura: USB e HID para o projeto

Especificacoes em `data/specs/` (ver `SOURCES.md`). Paginas = numero da pagina do PDF (nao o rotulo impresso), conferidas por busca no texto extraido.

## Ordem sugerida (do mais curto ao mais fundo)
1. HID 1.11, secao 5 (modelo operacional) e 6.2.1-6.2.2 (descritor HID e descritor de relatorio): `hid/hid1_11.pdf` p.32-33 em diante; itens principais p.38.
2. HID 1.11, secao 7.2 (requisicoes de classe: Get/Set_Report, Idle, Protocol) p.60 e secao 8 (formato dos relatorios) p.65. E onde vive o "init" do DS3.
3. HUT 1.7: pagina Generic Desktop (Joystick, Game Pad, X/Y/Z, Hat Switch) p.33; Game Controls p.79; Button p.111.
4. USB 2.0, cap. 9 (framework do dispositivo): enumeracao p.271, requisicoes padrao p.278, descritores p.289. Cap. 5, transferencia por interrupcao p.76 (e assim que um pad entrega os relatorios).
5. USB 1.1, cap. 9 p.201: o mesmo framework, util porque pads sao dispositivos low/full speed (a camada do 1.1 dentro do 2.0).

## Perguntas do projeto e onde a spec responde
- Como o console sabe que um dispositivo e HID? Descritor de interface com bInterfaceClass = 3 (HID 1.11, secao 4 e 6.2.1). Dispositivo com classe 0xFF (vendor) nao e HID.
- Como descobre botoes e eixos? Pelo descritor de relatorio: Usage Page, Usage, Logical Min/Max, Report Size/Count, Input (HID 6.2.2).
- O que e o hat? Usage 0x39 da pagina Generic Desktop (HUT p.33, descricao p.40): valor variavel com estado NULL (neutro). Nos descritores dos logs do forum aparece como campo de 4 bits (8 direcoes + neutro); isso vem dos logs, nao do texto da spec.
- Como o host manda init ou vibracao? Set_Report (control transfer, HID 7.2.2) ou out por endpoint de interrupcao.
- O que e boot protocol? Modo simples de teclado/mouse (HID apendice B), nao usado por gamepads.
