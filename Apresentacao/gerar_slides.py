#!/usr/bin/env python3
"""Gera a apresentação da defesa a partir do modelo oficial do IFSC.

O arquivo ODP é montado como um pacote OpenDocument para preservar o mestre do
modelo oficial. As imagens são incorporadas ao pacote e o roteiro é gerado a
partir da mesma especificação usada nas notas do apresentador.
"""

from __future__ import annotations

import hashlib
import mimetypes
import re
import shutil
import zipfile
from dataclasses import dataclass
from pathlib import Path
from xml.sax.saxutils import escape

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "Apresentacao"
TEMPLATE = Path("/home/hexagon/Downloads/modelos_slides_apresentacao_ifsc/modelo_horizontal16x9.odp")
ODP_OUT = OUT_DIR / "Defesa_TCC_Guilherme_Franco.odp"
SCRIPT_OUT = OUT_DIR / "ROTEIRO_CRONOMETRADO.md"

W, H = 28.0, 15.75
GRAPHITE = "#263238"
GREEN = "#45982F"
TEAL = "#00796B"
BLUE = "#1565C0"
AMBER = "#F9A825"
RED = "#C62828"
LIGHT = "#F4F8F5"
PALE_BLUE = "#EDF4FC"
PALE_TEAL = "#EAF6F3"
PALE_AMBER = "#FFF7DF"
PALE_RED = "#FCEEEE"


NS = {
    "office": "urn:oasis:names:tc:opendocument:xmlns:office:1.0",
    "style": "urn:oasis:names:tc:opendocument:xmlns:style:1.0",
    "text": "urn:oasis:names:tc:opendocument:xmlns:text:1.0",
    "draw": "urn:oasis:names:tc:opendocument:xmlns:drawing:1.0",
    "fo": "urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0",
    "xlink": "http://www.w3.org/1999/xlink",
    "presentation": "urn:oasis:names:tc:opendocument:xmlns:presentation:1.0",
    "svg": "urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0",
}


def q(text: str) -> str:
    return escape(text, {'"': "&quot;"})


def cm(value: float) -> str:
    return f"{value:.3f}cm"


def mmss(seconds: int) -> str:
    return f"{seconds // 60:02d}:{seconds % 60:02d}"


@dataclass
class SlideInfo:
    number: int
    title: str
    seconds: int
    message: str
    speech: str
    transition: str
    cut: str = ""
    support: bool = False


class Deck:
    def __init__(self) -> None:
        self.pages: list[str] = []
        self.infos: list[SlideInfo] = []
        self.assets: dict[Path, str] = {}
        self.frame_id = 0

    def _id(self, prefix: str) -> str:
        self.frame_id += 1
        return f"{prefix}{self.frame_id}"

    def register_asset(self, source: Path) -> str:
        source = source.resolve()
        if source in self.assets:
            return self.assets[source]
        digest = hashlib.sha1(str(source).encode()).hexdigest()[:8]
        safe = re.sub(r"[^A-Za-z0-9_.-]", "_", source.name)
        name = f"Pictures/{digest}_{safe}"
        self.assets[source] = name
        return name

    def text(self, x: float, y: float, w: float, h: float,
             lines: list[tuple[str, str]] | tuple[str, str] | str,
             graphic: str = "gNone") -> str:
        if isinstance(lines, str):
            lines = [("pBody", lines)]
        elif isinstance(lines, tuple):
            lines = [lines]
        paras = "".join(f'<text:p text:style-name="{style}">{q(txt)}</text:p>' for style, txt in lines)
        return (
            f'<draw:frame draw:name="{self._id("Text")}" draw:style-name="{graphic}" '
            f'svg:x="{cm(x)}" svg:y="{cm(y)}" svg:width="{cm(w)}" svg:height="{cm(h)}">'
            f'<draw:text-box>{paras}</draw:text-box></draw:frame>'
        )

    def rect(self, x: float, y: float, w: float, h: float, style: str = "gCard") -> str:
        return (
            f'<draw:rect draw:name="{self._id("Rect")}" draw:style-name="{style}" '
            f'svg:x="{cm(x)}" svg:y="{cm(y)}" svg:width="{cm(w)}" svg:height="{cm(h)}"/>'
        )

    def card(self, x: float, y: float, w: float, h: float,
             title: str, body: str = "", metric: str = "",
             style: str = "gCard", metric_style: str = "pMetric",
             title_style: str = "pCardTitle", title_height: float = 0.70,
             metric_height: float = 0.85, body_style: str = "pCardBody") -> str:
        parts = [self.rect(x, y, w, h, style)]
        ty = y + 0.32
        parts.append(self.text(x + 0.35, ty, w - 0.70, title_height, (title_style, title)))
        ty += title_height + 0.08
        if metric:
            parts.append(self.text(x + 0.35, ty, w - 0.70, metric_height, (metric_style, metric)))
            ty += metric_height + 0.07
        if body:
            parts.append(self.text(x + 0.35, ty, w - 0.70, max(0.55, y + h - ty - 0.25),
                                   [(body_style, line) for line in body.split("\n")]))
        return "".join(parts)

    def image(self, source: Path, x: float, y: float, w: float, h: float,
              border: bool = False) -> str:
        href = self.register_asset(source)
        with Image.open(source) as im:
            iw, ih = im.size
        scale = min(w / iw, h / ih)
        fw, fh = iw * scale, ih * scale
        fx, fy = x + (w - fw) / 2, y + (h - fh) / 2
        style = "gImageBorder" if border else "gImage"
        return (
            f'<draw:frame draw:name="{self._id("Image")}" draw:style-name="{style}" '
            f'svg:x="{cm(fx)}" svg:y="{cm(fy)}" svg:width="{cm(fw)}" svg:height="{cm(fh)}">'
            f'<draw:image xlink:href="{q(href)}" xlink:type="simple" xlink:show="embed" '
            f'xlink:actuate="onLoad"/></draw:frame>'
        )

    def arrow(self, x: float, y: float, w: float = 1.0, h: float = 0.8) -> str:
        return self.text(x, y, w, h, ("pArrow", "→"))

    def title(self, title: str, support: bool = False) -> str:
        parts = [self.text(4.35, 0.92, 22.0, 1.40, ("pTitle", title))]
        if support:
            parts.append(self.text(22.3, 2.18, 4.0, 0.42, ("pSupport", "APOIO TÉCNICO")))
        return "".join(parts)

    def source(self, text: str) -> str:
        return self.text(1.2, 14.42, 25.5, 0.55, ("pSource", text))

    def add_slide(self, info: SlideInfo, body: str, source: str = "") -> None:
        # Os slides de apoio já recebem a identificação no cabeçalho criado por
        # ``title(..., support=True)``. Mantê-la aqui a duplicaria na renderização.
        support_tag = ""
        notes_lines = [
            ("pNoteHead", f"Tempo-alvo: {mmss(info.seconds)}" if info.seconds else "Slide de apoio"),
            ("pNoteHead", f"Mensagem: {info.message}"),
            ("pNote", info.speech),
            ("pNoteHead", f"Transição: {info.transition}"),
        ]
        if info.cut:
            notes_lines.append(("pNoteCut", f"Corte: {info.cut}"))
        note_paras = "".join(f'<text:p text:style-name="{s}">{q(t)}</text:p>' for s, t in notes_lines)
        notes = (
            '<presentation:notes draw:style-name="dp2">'
            f'<draw:frame draw:name="{self._id("Notes")}" draw:style-name="gNotes" '
            'svg:x="2.1cm" svg:y="14.107cm" svg:width="16.799cm" svg:height="13.364cm" '
            'presentation:class="notes"><draw:text-box>'
            f'{note_paras}</draw:text-box></draw:frame></presentation:notes>'
        )
        page = (
            f'<draw:page draw:name="page{info.number}" draw:style-name="dp1" '
            'draw:master-page-name="Padrão" presentation:presentation-page-layout-name="AL1T1">'
            f'{body}{support_tag}{self.source(source) if source else ""}{notes}</draw:page>'
        )
        self.pages.append(page)
        self.infos.append(info)

    def content_xml(self) -> str:
        ns_attrs = " ".join(f'xmlns:{p}="{u}"' for p, u in NS.items())
        styles = f"""
<style:style style:name="dp1" style:family="drawing-page"><style:drawing-page-properties presentation:background-visible="true" presentation:background-objects-visible="true" presentation:display-footer="true" presentation:display-page-number="false" presentation:display-date-time="false"/></style:style>
<style:style style:name="dp2" style:family="drawing-page"><style:drawing-page-properties presentation:display-header="true" presentation:display-footer="true"/></style:style>
<style:style style:name="gNone" style:family="graphic"><style:graphic-properties draw:stroke="none" draw:fill="none" fo:padding="0cm" draw:textarea-vertical-align="top"/></style:style>
<style:style style:name="gImage" style:family="graphic"><style:graphic-properties draw:stroke="none" draw:fill="none"/></style:style>
<style:style style:name="gImageBorder" style:family="graphic"><style:graphic-properties draw:stroke="solid" svg:stroke-width="0.035cm" svg:stroke-color="{GREEN}" draw:fill="none"/></style:style>
<style:style style:name="gCard" style:family="graphic"><style:graphic-properties draw:stroke="solid" svg:stroke-width="0.035cm" svg:stroke-color="#B7C8BE" draw:fill="solid" draw:fill-color="{LIGHT}" draw:corner-radius="0.18cm"/></style:style>
<style:style style:name="gBlue" style:family="graphic"><style:graphic-properties draw:stroke="solid" svg:stroke-width="0.035cm" svg:stroke-color="{BLUE}" draw:fill="solid" draw:fill-color="{PALE_BLUE}" draw:corner-radius="0.18cm"/></style:style>
<style:style style:name="gTeal" style:family="graphic"><style:graphic-properties draw:stroke="solid" svg:stroke-width="0.035cm" svg:stroke-color="{TEAL}" draw:fill="solid" draw:fill-color="{PALE_TEAL}" draw:corner-radius="0.18cm"/></style:style>
<style:style style:name="gAmber" style:family="graphic"><style:graphic-properties draw:stroke="solid" svg:stroke-width="0.035cm" svg:stroke-color="{AMBER}" draw:fill="solid" draw:fill-color="{PALE_AMBER}" draw:corner-radius="0.18cm"/></style:style>
<style:style style:name="gRed" style:family="graphic"><style:graphic-properties draw:stroke="solid" svg:stroke-width="0.035cm" svg:stroke-color="{RED}" draw:fill="solid" draw:fill-color="{PALE_RED}" draw:corner-radius="0.18cm"/></style:style>
<style:style style:name="gDark" style:family="graphic"><style:graphic-properties draw:stroke="none" draw:fill="solid" draw:fill-color="{GRAPHITE}" draw:corner-radius="0.18cm"/></style:style>
<style:style style:name="gNotes" style:family="graphic"><style:graphic-properties draw:stroke="none" draw:fill="solid" draw:fill-color="#FFFFFF"/></style:style>

<style:style style:name="pTitle" style:family="paragraph"><style:paragraph-properties fo:text-align="start"/><style:text-properties style:font-name="Arial" fo:font-family="Arial" fo:font-size="30pt" fo:font-weight="bold" fo:color="{GREEN}"/></style:style>
<style:style style:name="pCover" style:family="paragraph"><style:paragraph-properties fo:text-align="start" fo:margin-bottom="0.10cm"/><style:text-properties style:font-name="Arial" fo:font-size="25pt" fo:font-weight="bold" fo:color="{GRAPHITE}"/></style:style>
<style:style style:name="pCoverSub" style:family="paragraph"><style:paragraph-properties fo:text-align="start" fo:margin-bottom="0.10cm"/><style:text-properties style:font-name="Arial" fo:font-size="17pt" fo:color="{TEAL}"/></style:style>
<style:style style:name="pBody" style:family="paragraph"><style:paragraph-properties fo:text-align="start" fo:margin-bottom="0.15cm"/><style:text-properties style:font-name="Arial" fo:font-size="22pt" fo:color="{GRAPHITE}"/></style:style>
<style:style style:name="pBodyBold" style:family="paragraph"><style:paragraph-properties fo:text-align="start" fo:margin-bottom="0.12cm"/><style:text-properties style:font-name="Arial" fo:font-size="22pt" fo:font-weight="bold" fo:color="{GRAPHITE}"/></style:style>
<style:style style:name="pCenter" style:family="paragraph"><style:paragraph-properties fo:text-align="center"/><style:text-properties style:font-name="Arial" fo:font-size="19pt" fo:color="{GRAPHITE}"/></style:style>
<style:style style:name="pCenterBold" style:family="paragraph"><style:paragraph-properties fo:text-align="center"/><style:text-properties style:font-name="Arial" fo:font-size="20pt" fo:font-weight="bold" fo:color="{GRAPHITE}"/></style:style>
<style:style style:name="pCardTitle" style:family="paragraph"><style:paragraph-properties fo:text-align="start" fo:margin-bottom="0.07cm"/><style:text-properties style:font-name="Arial" fo:font-size="18pt" fo:font-weight="bold" fo:color="{TEAL}"/></style:style>
<style:style style:name="pCardTitleSmall" style:family="paragraph"><style:paragraph-properties fo:text-align="start" fo:margin-bottom="0.06cm"/><style:text-properties style:font-name="Arial" fo:font-size="15pt" fo:font-weight="bold" fo:color="{TEAL}"/></style:style>
<style:style style:name="pCardBody" style:family="paragraph"><style:paragraph-properties fo:text-align="start" fo:margin-bottom="0.08cm"/><style:text-properties style:font-name="Arial" fo:font-size="16pt" fo:color="{GRAPHITE}"/></style:style>
<style:style style:name="pCardBodySmall" style:family="paragraph"><style:paragraph-properties fo:text-align="start" fo:margin-bottom="0.06cm"/><style:text-properties style:font-name="Arial" fo:font-size="14pt" fo:color="{GRAPHITE}"/></style:style>
<style:style style:name="pCardLabel" style:family="paragraph"><style:paragraph-properties fo:text-align="start" fo:margin-bottom="0.05cm"/><style:text-properties style:font-name="Arial" fo:font-size="12.5pt" fo:font-weight="bold" fo:color="{TEAL}"/></style:style>
<style:style style:name="pMetric" style:family="paragraph"><style:paragraph-properties fo:text-align="start"/><style:text-properties style:font-name="Arial" fo:font-size="27pt" fo:font-weight="bold" fo:color="{BLUE}"/></style:style>
<style:style style:name="pMetricTeal" style:family="paragraph"><style:paragraph-properties fo:text-align="start"/><style:text-properties style:font-name="Arial" fo:font-size="27pt" fo:font-weight="bold" fo:color="{TEAL}"/></style:style>
<style:style style:name="pMetricAmber" style:family="paragraph"><style:paragraph-properties fo:text-align="start"/><style:text-properties style:font-name="Arial" fo:font-size="27pt" fo:font-weight="bold" fo:color="{AMBER}"/></style:style>
<style:style style:name="pMetricRed" style:family="paragraph"><style:paragraph-properties fo:text-align="start"/><style:text-properties style:font-name="Arial" fo:font-size="27pt" fo:font-weight="bold" fo:color="{RED}"/></style:style>
<style:style style:name="pMetricCompact" style:family="paragraph"><style:paragraph-properties fo:text-align="start"/><style:text-properties style:font-name="Arial" fo:font-size="20pt" fo:font-weight="bold" fo:color="{BLUE}"/></style:style>
<style:style style:name="pMetricCompactTeal" style:family="paragraph"><style:paragraph-properties fo:text-align="start"/><style:text-properties style:font-name="Arial" fo:font-size="20pt" fo:font-weight="bold" fo:color="{TEAL}"/></style:style>
<style:style style:name="pMetricCompactAmber" style:family="paragraph"><style:paragraph-properties fo:text-align="start"/><style:text-properties style:font-name="Arial" fo:font-size="20pt" fo:font-weight="bold" fo:color="{AMBER}"/></style:style>
<style:style style:name="pBig" style:family="paragraph"><style:paragraph-properties fo:text-align="center"/><style:text-properties style:font-name="Arial" fo:font-size="34pt" fo:font-weight="bold" fo:color="{GREEN}"/></style:style>
<style:style style:name="pQuote" style:family="paragraph"><style:paragraph-properties fo:text-align="center"/><style:text-properties style:font-name="Arial" fo:font-size="23pt" fo:font-weight="bold" fo:color="{GRAPHITE}"/></style:style>
<style:style style:name="pArrow" style:family="paragraph"><style:paragraph-properties fo:text-align="center"/><style:text-properties style:font-name="Arial" fo:font-size="30pt" fo:font-weight="bold" fo:color="{AMBER}"/></style:style>
<style:style style:name="pSmall" style:family="paragraph"><style:paragraph-properties fo:text-align="start" fo:margin-bottom="0.08cm"/><style:text-properties style:font-name="Arial" fo:font-size="13pt" fo:color="{GRAPHITE}"/></style:style>
<style:style style:name="pTiny" style:family="paragraph"><style:paragraph-properties fo:text-align="start" fo:margin-bottom="0.05cm"/><style:text-properties style:font-name="Arial" fo:font-size="10.5pt" fo:color="{GRAPHITE}"/></style:style>
<style:style style:name="pRef" style:family="paragraph"><style:paragraph-properties fo:text-align="start" fo:margin-bottom="0.10cm"/><style:text-properties style:font-name="Arial" fo:font-size="12pt" fo:color="{GRAPHITE}"/></style:style>
<style:style style:name="pSource" style:family="paragraph"><style:paragraph-properties fo:text-align="start"/><style:text-properties style:font-name="Arial" fo:font-size="10pt" fo:color="#5F6B70"/></style:style>
<style:style style:name="pSupport" style:family="paragraph"><style:paragraph-properties fo:text-align="end"/><style:text-properties style:font-name="Arial" fo:font-size="10pt" fo:font-weight="bold" fo:color="{TEAL}"/></style:style>
<style:style style:name="pNoteHead" style:family="paragraph"><style:paragraph-properties fo:margin-bottom="0.12cm"/><style:text-properties style:font-name="Arial" fo:font-size="13pt" fo:font-weight="bold" fo:color="{TEAL}"/></style:style>
<style:style style:name="pNote" style:family="paragraph"><style:paragraph-properties fo:margin-bottom="0.15cm"/><style:text-properties style:font-name="Arial" fo:font-size="12pt" fo:color="{GRAPHITE}"/></style:style>
<style:style style:name="pNoteCut" style:family="paragraph"><style:paragraph-properties fo:margin-bottom="0.12cm"/><style:text-properties style:font-name="Arial" fo:font-size="12pt" fo:font-weight="bold" fo:color="{RED}"/></style:style>
"""
        return (
            '<?xml version="1.0" encoding="UTF-8"?>'
            f'<office:document-content {ns_attrs} office:version="1.2">'
            '<office:scripts/><office:font-face-decls>'
            '<style:font-face style:name="Arial" svg:font-family="Arial" style:font-family-generic="swiss" style:font-pitch="variable"/>'
            '<style:font-face style:name="Liberation Sans" svg:font-family="Liberation Sans" style:font-family-generic="swiss" style:font-pitch="variable"/>'
            f'</office:font-face-decls><office:automatic-styles>{styles}</office:automatic-styles>'
            f'<office:body><office:presentation>{"".join(self.pages)}'
            '<presentation:settings presentation:mouse-visible="false"/>'
            '</office:presentation></office:body></office:document-content>'
        )

    def write_odp(self) -> None:
        with zipfile.ZipFile(TEMPLATE, "r") as zf:
            template_files = {name: zf.read(name) for name in zf.namelist()}

        manifest = template_files["META-INF/manifest.xml"].decode("utf-8")
        additions = []
        for source, href in self.assets.items():
            mime = mimetypes.guess_type(source.name)[0] or "application/octet-stream"
            additions.append(
                f'<manifest:file-entry manifest:full-path="{q(href)}" manifest:media-type="{q(mime)}"/>'
            )
        manifest = manifest.replace("</manifest:manifest>", "".join(additions) + "</manifest:manifest>")

        tmp = ODP_OUT.with_suffix(".odp.tmp")
        with zipfile.ZipFile(tmp, "w") as out:
            out.writestr("mimetype", template_files["mimetype"], compress_type=zipfile.ZIP_STORED)
            for name, data in template_files.items():
                if name in {"mimetype", "content.xml", "META-INF/manifest.xml"}:
                    continue
                out.writestr(name, data, compress_type=zipfile.ZIP_DEFLATED)
            out.writestr("content.xml", self.content_xml().encode("utf-8"), compress_type=zipfile.ZIP_DEFLATED)
            out.writestr("META-INF/manifest.xml", manifest.encode("utf-8"), compress_type=zipfile.ZIP_DEFLATED)
            for source, href in self.assets.items():
                out.write(source, href, compress_type=zipfile.ZIP_DEFLATED)
        shutil.move(tmp, ODP_OUT)

    def write_script(self) -> None:
        main = [s for s in self.infos if not s.support]
        assert sum(s.seconds for s in main) == 1200, sum(s.seconds for s in main)
        lines = [
            "# Roteiro cronometrado — defesa do TCC",
            "",
            "**Duração total planejada:** 20:00  ",
            "**Demonstração ao vivo:** 1:50  ",
            "**Termo principal:** plataforma vestível modular multissensor",
            "",
            "## Preparação antes da banca",
            "",
            "- Carregar a bateria e manter um cabo USB disponível como contingência.",
            "- Ligar o protótipo na tela principal antes do início.",
            "- Calibrar a bússola no local, afastada de notebook, projetor e estruturas metálicas.",
            "- Limpar o MAX30102 e testar a posição do dedo.",
            "- Abrir ODP e PPTX no computador da apresentação e confirmar fontes e proporção 16:9.",
            "- Ensaiar a rota estática do slide 13 caso a demonstração seja interrompida.",
            "",
            "## Exposição principal",
            "",
        ]
        elapsed = 0
        for s in main:
            start = elapsed
            elapsed += s.seconds
            lines += [
                f"### {s.number}. {s.title} — {mmss(s.seconds)} ({mmss(start)}–{mmss(elapsed)})",
                "",
                f"**Mensagem:** {s.message}",
                "",
                s.speech,
                "",
                f"**Transição:** {s.transition}",
            ]
            if s.cut:
                lines += ["", f"**Corte se atrasado:** {s.cut}"]
            lines.append("")
        lines += [
            "## Sinais de tempo",
            "",
            "- **18:30:** concluir imediatamente o roadmap e entrar nas conclusões.",
            "- **19:30:** usar a versão curta da conclusão: objetivo atingido, arquitetura implementada e evidências delimitadas.",
            "- **19:50:** abrir o slide de perguntas, sem acrescentar nova explicação.",
            "",
            "## Slides de apoio",
            "",
        ]
        for s in [x for x in self.infos if x.support]:
            lines += [f"- **{s.number}. {s.title}:** {s.message}"]
        lines += [
            "",
            "## Critério de ensaio",
            "",
            "Executar pelo menos três ensaios completos. Nenhum pode ultrapassar 20:00. Registrar o tempo de cada slide e reduzir primeiro os slides 4, 9, 16 e 18 se houver atraso.",
        ]
        SCRIPT_OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")


def asset(relative: str) -> Path:
    path = ROOT / relative
    if not path.exists():
        raise FileNotFoundError(path)
    return path


def make_deck() -> Deck:
    d = Deck()

    # 1 — Capa
    info = SlideInfo(1, "Capa", 25,
        "Apresentar o trabalho como uma solução de integração embarcada, não como um produto comercial.",
        "Boa tarde. Este trabalho apresenta o desenvolvimento de uma plataforma vestível modular multissensor baseada no ESP32-C6. O foco não foi apenas reunir sensores em um dispositivo: foi organizar hardware, tarefas, interface e serviços compartilhados de forma que os módulos permanecessem compreensíveis e que uma ausência local não impedisse as funções independentes.",
        "Começo pelo problema que motivou essa arquitetura.")
    body = "".join([
        d.text(4.6, 3.00, 14.7, 3.2, [("pCover", "Desenvolvimento de uma plataforma"),
                                      ("pCover", "vestível modular multissensor"),
                                      ("pCover", "baseada no ESP32-C6")]),
        d.text(4.7, 7.05, 13.7, 3.5, [("pCoverSub", "Guilherme da Costa Franco"),
                                      ("pBody", "Engenharia Eletrônica"),
                                      ("pSmall", "Orientador: Prof. Me. Leandro Schwarz"),
                                      ("pSmall", "IFSC — Câmpus Florianópolis · 2026")]),
        d.image(asset("Imagens/PCB/Prototipo.jpeg"), 19.5, 3.15, 6.0, 10.3, border=True),
    ])
    d.add_slide(info, body, "Fonte da fotografia: acervo do autor (2026).")

    # 2 — Problema
    info = SlideInfo(2, "Problema de engenharia", 65,
        "O problema surge da interação entre módulos, não do funcionamento isolado de cada sensor.",
        "Os sensores usam protocolos e cadências diferentes. No sistema integrado, eles passam a disputar barramento, tempo de CPU, memória e espaço de interface. Uma falha de comunicação ou uma dependência espalhada pode deixar de ser local e afetar todo o firmware. Portanto, o problema foi integrar essa heterogeneidade sem concentrar o processamento no núcleo e sem permitir que a ausência de um sensor externo bloqueasse as demais funções.",
        "A partir desse problema, defini um objetivo orientado à arquitetura e à evidência.")
    body = d.title("Problema de engenharia")
    body += d.card(1.4, 3.25, 5.3, 3.0, "Sinais fisiológicos", "MAX30102\nPPG · FC · SpO₂", style="gRed",
                   title_style="pCardTitleSmall", title_height=0.95, body_style="pCardBodySmall")
    body += d.card(1.4, 7.00, 5.3, 3.35, "Ambiente", "DS18B20 · LTR390\ntemperatura · luz · UV", style="gAmber", body_style="pCardBodySmall")
    body += d.card(1.4, 10.75, 5.3, 2.8, "Movimento", "MPU/AK8963\nheading · passos", style="gBlue", body_style="pCardBodySmall")
    body += d.arrow(7.0, 7.15, 1.2, 1.0)
    body += d.card(8.1, 5.15, 8.1, 5.6, "Recursos comuns", "I²C e SPI\nFreeRTOS e prioridades\nLVGL e memória\nNVS e RTC", style="gTeal", metric="INTEGRAÇÃO", metric_style="pMetricTeal")
    body += d.arrow(16.45, 7.15, 1.2, 1.0)
    body += d.card(17.7, 4.15, 8.2, 7.6, "Riscos sistêmicos", "• propagação de NACK\n• interface sem resposta\n• dependências no núcleo\n• falha local interromper o sistema", style="gRed")
    body += d.text(8.0, 12.15, 17.8, 1.2, ("pQuote", "Falha local não deve interromper funções independentes."))
    d.add_slide(info, body, "Fonte: elaborado pelo autor a partir do problema de pesquisa.")

    # 3 — Objetivo e contribuições
    info = SlideInfo(3, "Objetivo e contribuição", 50,
        "A contribuição é a organização concreta da integração, sustentada por protótipo e evidências.",
        "O objetivo foi projetar e implementar a plataforma, integrando sensores heterogêneos, interface e serviços compartilhados, e observar estruturalmente a extensibilidade e o comportamento com módulos ausentes. A contribuição aparece em três frentes: uma arquitetura modular executável, um protótipo físico completo e uma avaliação que separa o que foi medido, o que foi observado qualitativamente e o que permaneceu fora do escopo.",
        "Antes do firmware, apresento o hardware que efetivamente foi construído.")
    body = d.title("Objetivo e contribuição")
    body += d.card(1.4, 3.15, 25.2, 2.55, "Objetivo geral",
                   "Integrar sensores, interface e serviços compartilhados em uma plataforma vestível modular baseada no ESP32-C6.", style="gTeal")
    body += d.card(1.4, 6.55, 7.7, 5.6, "Arquitetura", "Contrato init/create/show\nDriver, processamento e tela\nServiços compartilhados no núcleo", metric="MODULAR", style="gBlue")
    body += d.card(10.15, 6.55, 7.7, 5.6, "Implementação", "Placa protótipo autoral\nInvólucro impresso em 3D\nQuatro páginas de navegação", metric="EXECUTÁVEL", metric_style="pMetricTeal", style="gTeal")
    body += d.card(18.90, 6.55, 7.7, 5.6, "Avaliação", "Build e análise estática\nEnsaios funcionais\nLimitações explicitadas", metric="RASTREÁVEL", metric_style="pMetricAmber", style="gAmber")
    d.add_slide(info, body, "Fonte: elaborado pelo autor a partir dos objetivos e contribuições do trabalho.")

    # 4 — Protótipo
    info = SlideInfo(4, "Protótipo realizado", 60,
        "Placa e invólucro pertencem ao resultado implementado, não ao roadmap.",
        "A integração deixou a protoboard e foi transferida para uma placa protótipo autoral. O XIAO ESP32-C6 permaneceu acoplado ao Round Display e os módulos foram distribuídos na placa. Também foi fabricado um invólucro por impressão 3D. As renderizações mostram as duas faces previstas e a fotografia registra a placa em funcionamento. A caracterização mecânica e ambiental do invólucro não integrou os ensaios, mas a construção foi realizada.",
        "O desenvolvimento desse conjunto seguiu um ciclo incremental e regressivo.",
        "Se o tempo estiver acima do planejado, omitir a descrição individual das faces da placa.")
    body = d.title("Protótipo realizado")
    body += d.image(asset("Imagens/PCB/Prototipo.jpeg"), 1.35, 3.05, 7.0, 10.7, border=True)
    body += d.image(asset("Imagens/PCB/3dtoplayer.png"), 9.05, 3.15, 7.7, 7.7, border=True)
    body += d.image(asset("Imagens/PCB/3dbottomlayer.png"), 17.35, 3.15, 7.7, 7.7, border=True)
    body += d.text(9.0, 11.05, 16.1, 1.9, [("pCenterBold", "Placa protótipo autoral + invólucro impresso em 3D"),
                                           ("pCenter", "Elementos realizados na versão final")])
    d.add_slide(info, body, "Fonte: acervo e projeto do autor (2026).")

    # 5 — Método: visão completa
    info = SlideInfo(5, "Método incremental — visão completa", 35,
        "O método articula evidência isolada, integração e regressão em um ciclo de retorno.",
        "Esta é a visão completa do método. Ele está organizado em três faixas de evidência: primeiro o componente é compreendido e verificado isoladamente; depois suas responsabilidades são separadas e ele é integrado à plataforma; por fim, as funções existentes passam por regressão. Os retornos tracejados mostram que um conflito faz o trabalho voltar à etapa responsável, em vez de seguir adiante com uma integração instável.",
        "Amplio primeiro a faixa que produz a evidência de teste isolado.")
    body = d.title("Método incremental")
    body += d.image(asset("Imagens/Diagramas/teoricas/metodo_incremental.png"), 1.35, 3.0, 25.3, 10.9)
    d.add_slide(info, body, "Fonte: elaborado pelo autor a partir do método empregado no projeto.")

    # 6 — Método: evidência isolada
    info = SlideInfo(6, "Método — teste isolado", 35,
        "O módulo só avança após aquisição e tratamento funcionarem fora do firmware integrado.",
        "O primeiro recorte começa no datasheet e na interface elétrica ou lógica do componente. Em seguida, o teste standalone permite controlar configuração, comunicação e dados brutos com poucas variáveis externas. A terceira etapa verifica aquisição e tratamento isolados e produz uma evidência específica daquele módulo. Essa separação ajuda a distinguir um defeito do sensor ou do driver de um conflito introduzido pela integração.",
        "Com a evidência isolada, o trabalho passa à estruturação, integração e regressão.")
    body = d.title("Método: evidência de teste isolado")
    body += d.image(asset("Apresentacao/recortes/metodo_teste_isolado.png"), 1.30, 3.05, 25.4, 4.75)
    body += d.card(1.40, 8.55, 7.70, 3.55, "Compreender", "interface · configuração\nrestrições de operação", style="gBlue")
    body += d.card(10.15, 8.55, 7.70, 3.55, "Controlar variáveis", "firmware standalone\nlogs e dados brutos", style="gBlue")
    body += d.card(18.90, 8.55, 7.70, 3.55, "Critério de avanço", "aquisição e tratamento\nverificados isoladamente", style="gBlue")
    d.add_slide(info, body, "Fonte: elaborado pelo autor a partir do método empregado no projeto.")

    # 7 — Método: integração e regressão
    info = SlideInfo(7, "Método — integração e regressão", 35,
        "A integração separa responsabilidades e sempre termina em regressão das funções existentes.",
        "No segundo recorte, o código é dividido entre driver, processamento e apresentação, adaptado ao contrato comum e integrado ao núcleo e à interface. Depois vêm inicialização, resposta da interface, logs e continuidade das funções já existentes. Se aparecer conflito de barramento, temporização, memória ou interface, o diagnóstico define o ponto de retorno. Esse ciclo foi influenciado pela minha experiência com testes e regressão em sistemas embarcados.",
        "O resultado desse processo é a arquitetura implementada.")
    body = d.title("Método: integração, regressão e retorno")
    body += d.image(asset("Apresentacao/recortes/metodo_integracao_regressao.png"), 1.30, 3.0, 25.4, 10.15)
    d.add_slide(info, body, "Fonte: elaborado pelo autor a partir do método empregado no projeto.")

    # 8 — Arquitetura
    info = SlideInfo(8, "Arquitetura implementada", 85,
        "O núcleo compõe e coordena; cada módulo conserva hardware, processamento, estado e tela.",
        "No nível superior, o núcleo inicializa serviços, cria menus e despacha a atualização da tela ativa. Os módulos sensoriais possuem driver, processamento e apresentação próprios. O I²C, o LVGL e a NVS aparecem como serviços compartilhados. O contrato recorrente é inicializar, criar e exibir. Essa composição é explícita em main.c: trata-se de modularidade estática e localizada, não de um sistema de plugins descobertos automaticamente.",
        "Como o ESP32-C6 está em configuração unicore, essa arquitetura também depende de concorrência bem coordenada.")
    body = d.title("Arquitetura implementada")
    body += d.image(asset("Imagens/Diagramas/sistema/arquitetura_componentes_sistema.png"), 1.25, 2.90, 25.5, 11.25)
    d.add_slide(info, body, "Fonte: elaborado pelo autor a partir do firmware integrado.")

    # 9 — Concorrência e I2C
    info = SlideInfo(9, "Concorrência e barramento compartilhado", 80,
        "Prioridades preservam a interface; mutex e recuperação protegem o recurso compartilhado.",
        "O FreeRTOS opera em um único núcleo. A tarefa gráfica tem prioridade 4, as aquisições prioridade 3 e a captura prioridade 2. O I²C é compartilhado pelo toque, RTC e sensores. Cada driver recebe o mesmo mutex, que oferece herança de prioridade. Após erro de transação, o firmware reinicializa o controlador do barramento. Antes de criar o driver, também tenta liberar SDA presa com pulsos em SCL e uma condição de parada.",
        "Com essa infraestrutura, a integração de módulos pode ser analisada de forma localizada.",
        "Se houver atraso, citar apenas prioridades 4 e 3 e resumir a recuperação em uma frase.")
    body = d.title("Concorrência e barramento compartilhado")
    body += d.image(asset("Imagens/Diagramas/sistema/freertos_tasks.png"), 1.20, 3.05, 15.4, 9.5)
    body += d.card(17.2, 3.25, 8.8, 2.15, "1. Exclusão", "task → mutex I²C", style="gBlue")
    body += d.arrow(20.9, 5.30, 1.2, 0.7)
    body += d.card(17.2, 6.05, 8.8, 2.15, "2. Transação", "sucesso ou NACK", style="gTeal")
    body += d.arrow(20.9, 8.10, 1.2, 0.7)
    body += d.card(17.2, 8.85, 8.8, 2.15, "3. Recuperação", "i2c_master_bus_reset()", style="gRed")
    body += d.arrow(20.9, 10.90, 1.2, 0.7)
    body += d.card(17.2, 11.65, 8.8, 1.8, "4. Liberação", "próxima transação", style="gAmber")
    d.add_slide(info, body, "Fonte: elaborado pelo autor a partir de main.c, tarefas dos módulos e i2c_recover.c.")

    # 10 — Extensibilidade: visão completa
    info = SlideInfo(10, "Extensibilidade e contenção — visão completa", 40,
        "A modularidade foi sustentada por reutilização, pontos de integração explícitos e ensaios com sensores ausentes.",
        "A visão completa transforma o método em um procedimento concreto para acrescentar sensores. O fluxo parte dos requisitos, passa pelo teste isolado e pela composição explícita no núcleo, e termina em compilação, teste com o sensor ausente e regressão. Ao lado estão as três evidências usadas neste trabalho: 678 linhas reutilizadas, seis pontos externos de composição e operação preservada nos arranjos de sensores ausentes.",
        "Agora amplio como o módulo é preparado e composto na plataforma.")
    body = d.title("Extensibilidade e contenção de falhas")
    body += d.image(asset("Imagens/Diagramas/sistema/integracao_novo_sensor.png"), 1.20, 3.05, 16.6, 10.6)
    body += d.card(18.35, 3.25, 7.8, 2.65, "Reutilização", "6 arquivos preservados", metric="678 linhas", style="gBlue",
                   metric_style="pMetricCompact", metric_height=0.65)
    body += d.card(18.35, 6.30, 7.8, 2.65, "Composição explícita", "fora da pasta do módulo", metric="6 pontos", style="gTeal",
                   metric_style="pMetricCompactTeal", metric_height=0.65)
    body += d.card(18.35, 9.35, 7.8, 3.55, "Sensores ausentes", "remoção em arranjos\nfunções mantidas", metric="sem falha", style="gAmber",
                   metric_style="pMetricCompactAmber", metric_height=0.65)
    d.add_slide(info, body, "Fonte: firmware integrado e ensaios de sensores ausentes.")

    # 11 — Extensibilidade: preparar e compor
    info = SlideInfo(11, "Extensibilidade — preparar e compor", 40,
        "Um novo sensor entra pela borda do sistema e adere a contratos e serviços já definidos.",
        "Neste recorte, primeiro são definidos interface, alimentação, pinos, endereço e período de aquisição. O standalone verifica detecção, leitura e tratamento de erro. O módulo então separa hardware, processamento e tela. Se utilizar um recurso comum, recebe os handles e mecanismos existentes; se tiver transporte próprio, o acesso permanece encapsulado. Por fim, o núcleo compõe ciclo de vida, menu, callback, atualização da tela e diretórios de build. Os pontos externos são explícitos e localizados.",
        "A última faixa verifica se essa extensão permanece contida quando o sensor não está disponível.")
    body = d.title("Extensibilidade: preparar e compor")
    body += d.image(asset("Apresentacao/recortes/integracao_preparar_compor.png"), 1.30, 3.0, 25.4, 10.85)
    d.add_slide(info, body, "Fonte: elaborado pelo autor a partir do firmware integrado.")

    # 12 — Contenção de falhas e regressão
    info = SlideInfo(12, "Contenção de falhas e regressão", 40,
        "A ausência do sensor é testada antes de aprovar a regressão das funções existentes.",
        "Depois de compilar e inicializar, o procedimento remove o sensor e verifica se a indisponibilidade fica contida. No projeto, cada componente externo foi removido individualmente e também em diferentes arranjos. Nessas condições, não ocorreram panic, watchdog ou reinicialização espontânea, e as funções independentes continuaram operando. A aprovação não significa tolerância universal a falhas: significa que a ausência física ensaiada não bloqueou o núcleo nem os demais módulos. Se a regressão falhar, o fluxo retorna à etapa responsável.",
        "Com o caminho de extensão e contenção definido, mostro o sistema em execução.")
    body = d.title("Contenção de falhas e regressão")
    body += d.image(asset("Apresentacao/recortes/integracao_contencao_regressao.png"), 1.30, 3.05, 25.4, 5.15)
    body += d.card(1.40, 8.80, 7.70, 3.75, "Sensor ausente", "remoção individual e\nem diferentes arranjos", style="gAmber")
    body += d.card(10.15, 8.80, 7.70, 3.75, "Operação observada", "sem panic, watchdog ou\nreinicialização espontânea", style="gTeal")
    body += d.card(18.90, 8.80, 7.70, 3.75, "Regressão", "funções independentes\npermaneceram operantes", style="gBlue")
    d.add_slide(info, body, "Fonte: firmware integrado e ensaios de sensores ausentes.")

    # 13 — Demo
    info = SlideInfo(13, "Demonstração ao vivo", 110,
        "Mostrar a integração real por uma rota curta e previsível.",
        "A demonstração começa na tela principal. Abro o menu e percorro as quatro páginas para mostrar a organização funcional. Em VITAIS, abro o PPG e inicio a aquisição. Enquanto o estimador de DC converge, observo que a FIFO desacopla a taxa de amostragem das transações e que o processamento separa DC e AC em uma tarefa própria. Se não houver valor em doze segundos, sigo sem esperar. Depois retorno, abro a bússola e giro o protótipo para mostrar a resposta direcional.",
        "A demonstração mostra execução; agora apresento como essa solução foi avaliada.",
        "Se o protótipo falhar, narrar as três capturas deste slide em no máximo 25 segundos.")
    body = d.title("Demonstração ao vivo")
    shots = [
        ("Imagens/Telas Display/tela_230002_01_MenuPrincipal.png", "1. Interface e menus"),
        ("Imagens/Telas Display/tela_230056_03_MaxScreen.png", "2. Iniciar PPG"),
        ("Imagens/Telas Display/tela_230333_09_CompScreen.png", "3. Girar a bússola"),
    ]
    for i, (path, label) in enumerate(shots):
        x = 1.4 + i * 8.55
        body += d.card(x, 3.20, 7.6, 9.4, label, style=["gTeal", "gRed", "gBlue"][i])
        body += d.image(asset(path), x + 0.55, 4.55, 6.5, 6.5)
        if i < 2:
            body += d.arrow(x + 7.55, 7.0, 1.0, 1.0)
    body += d.text(5.0, 12.75, 18.0, 0.9, ("pQuote", "Rota-alvo: 1 min 50 s · contingência estática incorporada"))
    d.add_slide(info, body, "Fonte das capturas: firmware executável e acervo do autor (2026).")

    # 14 — Avaliação
    info = SlideInfo(14, "Como a solução foi avaliada", 75,
        "A avaliação combinou estrutura do código, enquadramento de recursos e ensaios funcionais.",
        "A análise estática verificou contrato, dependências e reutilização. O build limpo verificou o enquadramento global na partição: 784.240 bytes, ou 74,79 por cento de um mebibyte. Os ensaios funcionais verificaram interface, sensores, RTC, corrente e operação degradada. CPU e heap por cenário, falhas I²C induzidas e descarga completa não foram medidos e não são apresentados como resultados.",
        "Primeiro, mostro o resultado do módulo com o pipeline mais complexo: o PPG.")
    body = d.title("Como a solução foi avaliada")
    body += d.card(1.4, 3.25, 7.7, 8.8, "Análise estática", "Contrato e dependências\nReutilização do MAX30102\nPontos externos ao módulo", metric="678 linhas", style="gBlue")
    body += d.card(10.15, 3.25, 7.7, 8.8, "Build final", "Partição de aplicação: 1 MiB\nFolga: 264.336 bytes\nPool LVGL: 96 KiB", metric="74,79%", metric_style="pMetricTeal", style="gTeal")
    body += d.card(18.90, 3.25, 7.7, 8.8, "Ensaios", "Sensores e interface\nRTC e corrente total\nAusências sem impedir funções independentes", metric="sem falha", metric_style="pMetricAmber", style="gAmber")
    body += d.text(3.2, 12.55, 21.6, 0.85, ("pQuote", "Implementado ≠ ensaiado ≠ proposto"))
    d.add_slide(info, body, "Fonte: elaborado pelo autor a partir do build, análise do firmware e caderno de ensaios.")

    # 15 — PPG
    info = SlideInfo(15, "PPG: linha de base e espectro", 85,
        "A estimativa DC acompanhou a linha de base; o passa-baixa limitou a banda, mas teve efeito global discreto neste registro.",
        "No painel superior, o eixo vertical representa contagens do conversor, e não batimentos por minuto. Portanto, cerca de 86 mil contagens não significam 86 BPM: esse valor representa a intensidade óptica infravermelha recebida pelo sensor. A linha preta é o IR bruto e a linha âmbar é a estimativa DC, que acompanha as variações lentas da linha de base. A pulsação aparece como uma pequena modulação sobre esse nível elevado, e a frequência cardíaca é obtida pelo intervalo entre pulsos, não pela amplitude do ADC. No painel inferior, comparo o conteúdo espectral antes e depois do passa-baixa na mesma escala. Na faixa analisada, as curvas permanecem próximas; acima do corte de 5 hertz, a curva vermelha apresenta menor potência. A componente principal próxima de 1,40 hertz equivale a aproximadamente 84 BPM, porque 1,40 vezes 60 resulta em 84. Somente 2,2 por cento da energia do IR estava acima de 5 hertz, por isso o efeito global foi discreto. Entre 5 e 10 hertz, a potência foi reduzida em aproximadamente 5,7 decibéis, enquanto cerca de 98 por cento do RMS foi preservado. Assim, os gráficos comprovam o acompanhamento da linha de base e a limitação de banda, mas não demonstram ganho de exatidão no detector, pois ele não foi comparado de forma controlada com e sem esse filtro.",
        "Os demais sensores foram avaliados com protocolos proporcionais às suas grandezas.")
    body = d.title("PPG: linha de base e espectro")
    body += d.image(asset("Imagens/Diagramas/max30102/max30102_ppg_condicionamento_15_73s.png"), 1.15, 3.0, 18.1, 10.4)
    body += d.card(
        19.65, 3.55, 6.9, 7.35,
        "Síntese",
        "DC acompanha a linha de base\nIR acima de 5 Hz: 2,2%\nFC dominante: ≈84 BPM\nGanho no detector: não medido",
        style="gTeal", body_style="pCardBodySmall",
    )
    body += d.text(19.75, 11.45, 6.7, 1.55, ("pCenterBold", "Limitação de banda ≠ ganho de exatidão"))
    d.add_slide(info, body, "Fonte: elaborado pelo autor a partir do CSV do ensaio standalone; FC em BPM.")

    # 16 — Outros sensores
    info = SlideInfo(16, "Resultados dos demais sensores", 120,
        "Todos responderam às grandezas; o alcance de cada resultado permanece delimitado pelo procedimento.",
        "Na temperatura, o DS18B20 indicou 22,5 graus e o comparador 23,4, diferença de menos 0,9 grau em um único ponto, além de responder a aquecimento e resfriamento. O LTR390 acompanhou cerca de cinco ordens de grandeza e saturou próximo de 52 quilolux na configuração usada. Na bússola, as duas orientações diferiram 16 e 22 graus da comparação magnética. No percurso de 100 metros, a contagem manual foi 140 e o protótipo registrou 131 e 133 passos. São evidências funcionais, não generalizações populacionais ou metrológicas.",
        "Além dos sensores, foram observados RTC e consumo total do sistema.",
        "Se houver atraso, dizer apenas a métrica grande e a limitação de cada cartão.")
    body = d.title("Resultados dos demais sensores")
    body += d.card(1.4, 3.15, 11.9, 4.7, "DS18B20 — temperatura", "22,5 °C no protótipo × 23,4 °C no comparador\num ponto; resposta térmica funcional", metric="Δ −0,9 °C", style="gAmber", metric_style="pMetricAmber")
    body += d.card(14.1, 3.15, 11.9, 4.7, "LTR390 — iluminância", "do escuro ao sol direto\nteto da configuração de ganho 3× e 18 bits", metric="0 → ~52 klux", style="gTeal", metric_style="pMetricTeal")
    body += d.card(1.4, 8.55, 11.9, 4.7, "Bússola — heading", "comparação em duas orientações\nversão de ensaio referida ao norte magnético", metric="+16° / +22°", style="gBlue")
    body += d.card(14.1, 8.55, 11.9, 4.7, "Pedômetro — 100 m", "manual: 140 / 140\nprotótipo: erro médio de −5,7%", metric="131 / 133", style="gTeal", metric_style="pMetricTeal")
    d.add_slide(info, body, "Fonte: elaborado pelo autor a partir dos ensaios funcionais registrados.")

    # 17 — RTC e consumo
    info = SlideInfo(17, "RTC e consumo", 60,
        "O RTC manteve a base temporal; a corrente total variou conforme o cenário, sem ensaio de descarga.",
        "Depois de 24 horas com o sistema desligado, a bateria CR927 preservou o RTC e foi observado atraso próximo de dois segundos. A corrente total variou de 90 miliampères na tela PPG parada a 210 miliampères com o PPG medindo. Esses valores caracterizam cenários completos; não isolam o consumo de cada sensor. Como não houve descarga completa, não apresento autonomia medida.",
        "Essas limitações conduzem diretamente ao roadmap.")
    body = d.title("RTC e consumo")
    body += d.image(asset("Imagens/Diagramas/bateria/bateria_consumo_por_cenario.png"), 1.25, 3.0, 18.7, 10.4)
    body += d.card(20.4, 3.45, 5.7, 4.15, "RTC desligado", "CR927 manteve a contagem\num intervalo de 24 h", metric="~2 s", style="gBlue")
    body += d.card(20.4, 8.25, 5.7, 4.15, "Consumo total", "nove cenários medidos\nsem curva de descarga", metric="90–210 mA", metric_style="pMetricAmber", style="gAmber")
    d.add_slide(info, body, "Fonte: elaborado pelo autor a partir dos ensaios de RTC e corrente total.")

    # 18 — Roadmap
    info = SlideInfo(18, "Limitações e roadmap", 80,
        "Cada proposta futura deriva de uma limitação observada ou de uma expansão tecnicamente compatível.",
        "A primeira prioridade é caracterizar robustez: CPU, heap, memória incremental, falhas I²C induzidas, ensaio prolongado e descarga. Em energia, devem ser avaliados backlight, desligamento seletivo, sono e contador de coulombs. Em expansão, microSD exige resolver o conflito do GPIO20 com o DS18B20 e coordenar o SPI; Wi-Fi e Bluetooth exigem medir consumo e concorrência; RTK depende de antena e fonte de correções. Nos algoritmos, entram tilt compensation, calibração magnética matricial, persistência do pedômetro e qualidade do PPG.",
        "Com esse alcance definido, retomo o que foi efetivamente concluído.",
        "Aos 18:30, encerrar este slide após citar as quatro frentes, sem detalhar microSD ou RTK.")
    body = d.title("Limitações e roadmap")
    body += d.card(1.25, 3.20, 6.0, 9.7, "1. Robustez", "CPU e heap por cenário\nMemória incremental\nFalhas I²C induzidas\nEnsaio prolongado\nDescarga completa", style="gBlue")
    body += d.card(7.75, 3.20, 6.0, 9.7, "2. Energia", "Controle do backlight\nDesligamento seletivo\nModos de sono\nDespertar pelo RTC\nContador de coulombs", style="gAmber")
    body += d.card(14.25, 3.20, 6.0, 9.7, "3. Expansão", "microSD e histórico\nWi-Fi e Bluetooth\nGNSS com RTK\nSegurança e consumo\nCoordenação do SPI", style="gTeal")
    body += d.card(20.75, 3.20, 6.0, 9.7, "4. Algoritmos", "Tilt compensation\nSoft-iron matricial\nPedômetro adaptativo\nQualidade do PPG\nRegistro de módulos", style="gRed")
    d.add_slide(info, body, "Fonte: elaborado pelo autor a partir das limitações e trabalhos futuros.")

    # 19 — Conclusões
    info = SlideInfo(19, "Conclusões", 70,
        "O objetivo foi atingido como plataforma de integração modular, com evidências coerentes com o escopo.",
        "O trabalho entregou uma plataforma vestível funcional, com placa protótipo, invólucro e quatro páginas de interface. A arquitetura separou núcleo, serviços, drivers, processamento e telas. O estudo do MAX30102 mostrou reutilização localizada; o build coube na partição; e a remoção individual e combinada dos sensores externos não impediu as funções independentes nas condições ensaiadas. Os ensaios sensoriais demonstraram funcionamento e orientaram limitações concretas. Assim, a principal contribuição é uma base executável e evolutiva para integração multissensor no ESP32-C6.",
        "Obrigado. Fico à disposição para as perguntas.",
        "Aos 19:30, usar apenas: objetivo atingido, arquitetura implementada, build enquadrado e contenção funcional observada.")
    body = d.title("Conclusões")
    body += d.card(1.4, 3.30, 7.7, 7.6, "Integração realizada", "Placa e invólucro\nQuatro módulos sensoriais\nInterface e RTC", metric="executável", style="gTeal", metric_style="pMetricTeal")
    body += d.card(10.15, 3.30, 7.7, 7.6, "Arquitetura", "Reutilizadas sem alteração\nServiços compartilhados\nMudanças localizadas", metric="678 linhas", style="gBlue")
    body += d.card(18.90, 3.30, 7.7, 7.6, "Evidência", "Objetivo central\nBuild enquadrado\nEnsaios delimitados", metric="atingido", style="gAmber", metric_style="pMetricAmber")
    body += d.text(2.1, 11.75, 23.8, 1.2, ("pQuote", "Uma base concreta para evoluir a integração multissensor no ESP32-C6."))
    d.add_slide(info, body, "Fonte: síntese elaborada pelo autor.")

    # 20 — Perguntas
    info = SlideInfo(20, "Perguntas", 10,
        "Encerrar sem acrescentar conteúdo novo.",
        "Obrigado. Fico à disposição para as perguntas.",
        "Usar os slides de apoio conforme o tema levantado pela banca.")
    body = d.text(5.0, 3.35, 15.0, 2.0, ("pBig", "Obrigado"))
    body += d.text(5.0, 5.45, 15.0, 1.5, ("pQuote", "Perguntas?"))
    body += d.image(asset("Imagens/PCB/Prototipo.jpeg"), 20.2, 2.7, 5.4, 10.2, border=True)
    body += d.text(5.0, 8.4, 14.8, 2.5, [("pCoverSub", "Guilherme da Costa Franco"),
                                        ("pSmall", "Engenharia Eletrônica · IFSC")])
    d.add_slide(info, body, "Fonte da fotografia: acervo do autor (2026).")

    # 21 — apoio: navegação
    info = SlideInfo(21, "Mapa completo de navegação", 0,
        "Confirmar as quatro páginas e os retornos das telas.",
        "A interface possui quatro páginas: VITAIS, LUZ/UV, MOVIMENTO e CONFIGS. Cada tela retorna à página que a originou; calibração, hora e data são fluxos auxiliares.",
        "Retornar à pergunta da banca.", support=True)
    body = d.title("Mapa completo de navegação", support=True)
    body += d.image(asset("Imagens/Diagramas/teoricas/mapa_navegacao.png"), 1.2, 3.0, 25.5, 10.9)
    d.add_slide(info, body, "Fonte: elaborado pelo autor a partir dos callbacks e telas do firmware corrente.")

    # 22 — apoio: tarefas
    info = SlideInfo(22, "Tarefas, prioridades e cadências", 0,
        "Responder perguntas de RTOS sem afirmar paralelismo ou deadline rígido.",
        "FreeRTOS unicore, tick de 100 Hz. LVGL prioridade 4; aquisições prioridade 3; captura prioridade 2; idle prioridade 0. O despacho da tela ativa ocorre a cada 500 ms. Não houve análise formal de pior caso.",
        "Retornar à pergunta da banca.", support=True)
    body = d.title("Tarefas, prioridades e cadências", support=True)
    body += d.image(asset("Imagens/Diagramas/sistema/freertos_tasks.png"), 1.25, 3.0, 25.4, 10.8)
    d.add_slide(info, body, "Fonte: elaborado pelo autor a partir do firmware e sdkconfig.")

    # 23 — apoio: I2C
    info = SlideInfo(23, "I²C: elétrica, endereços e recuperação", 0,
        "Distinguir open-drain, exclusão mútua e recuperação do controlador.",
        "O barramento opera a 100 kHz em GPIO22/23. Open-drain depende de pull-up e tempo de subida RC. O mutex resolve concorrência de software; a recuperação trata linha/controlador após falha. A taxa de sucesso sob falhas deliberadamente induzidas não foi medida.",
        "Retornar à pergunta da banca.", support=True)
    body = d.title("I²C: elétrica, endereços e recuperação", support=True)
    body += d.image(asset("Imagens/Diagramas/teoricas/i2c_open_drain_rc.png"), 1.15, 3.0, 12.6, 10.7)
    body += d.image(asset("Imagens/Diagramas/sistema/i2c_recuperacao_nack.png"), 14.15, 3.0, 12.2, 10.7)
    d.add_slide(info, body, "Fonte: elaborado pelo autor com base no firmware e em NXP UM10204.")

    # 24 — apoio: parâmetros
    info = SlideInfo(24, "Parâmetros finais dos sensores", 0,
        "Fornecer os valores confirmados no firmware final.",
        "Usar esta lâmina apenas quando a banca pedir parâmetros. Não misturar valores dos projetos standalone com a integração final.",
        "Retornar à pergunta da banca.", support=True)
    body = d.title("Parâmetros finais dos sensores", support=True)
    rows = [
        ("MAX30102", "0x57 · 100 Hz · 18 bits", "SPO2_CONFIG 0x67", "LEDs 0x24 ≈ 7,2 mA"),
        ("DS18B20", "GPIO20 · 1-Wire/RMT", "11 bits · 375 ms", "EMA 0,50"),
        ("LTR390", "0x53 · 18 bits/100 ms", "ganhos ALS 3× / UVS 18×", "3 amostras de settling"),
        ("MPU/AK8963", "0x68 / 0x0C · 100 kHz", "magnetômetro 16 bits", "12 setores · NVS compass/cal"),
        ("Pedômetro", "~50 Hz · EMA 0,2", "limiar 1,15 g", "histerese 0,05 g · 300 ms"),
    ]
    y = 3.15
    for i, row in enumerate(rows):
        style = ["gBlue", "gAmber", "gTeal", "gBlue", "gTeal"][i]
        body += d.rect(1.35, y, 25.3, 1.85, style)
        body += d.text(1.70, y + 0.32, 4.4, 1.0, ("pCardTitle", row[0]))
        body += d.text(6.05, y + 0.32, 6.3, 1.0, ("pSmall", row[1]))
        body += d.text(12.45, y + 0.32, 6.2, 1.0, ("pSmall", row[2]))
        body += d.text(18.70, y + 0.32, 7.3, 1.0, ("pSmall", row[3]))
        y += 2.05
    d.add_slide(info, body, "Fonte: firmware integrado em Codigos/IDroid/main/.")

    # 25 — apoio: calibração
    info = SlideInfo(25, "Calibração magnética e limitações", 0,
        "Explicar o que a correção diagonal faz e o que ela não faz.",
        "A coleta cobre 12 setores e atualiza mínimos e máximos. Offsets recentram a nuvem e escalas diagonais equalizam os eixos. Não há matriz completa de soft-iron nem tilt compensation. Cancelar preserva a calibração válida.",
        "Retornar à pergunta da banca.", support=True)
    body = d.title("Calibração magnética e limitações", support=True)
    body += d.image(asset("Imagens/Diagramas/teoricas/calibracao_magnetica.png"), 1.10, 3.0, 12.7, 10.8)
    body += d.image(asset("Imagens/Diagramas/teoricas/calibracao_bussola_ondevice.png"), 14.15, 3.0, 12.3, 10.8)
    d.add_slide(info, body, "Fonte: elaborado pelo autor com base em Renaudin, Afzal e Lachapelle (2010) e no firmware.")

    # 26 — apoio: memória
    info = SlideInfo(26, "Memória e build final", 0,
        "Separar capacidade física, partição, imagem, mapa estático e heap dinâmico.",
        "A placa possui 4 MiB físicos; a configuração usa tabela single-app e partição de 1 MiB. O binário ocupa 784.240 bytes. O mapa estático de DIRAM não equivale ao heap livre, e não foi medido incremento por módulo.",
        "Retornar à pergunta da banca.", support=True)
    body = d.title("Memória e build final", support=True)
    metrics = [
        ("Partição", "1.048.576 bytes", "gBlue", "pMetric"),
        ("Binário gravável", "784.240 bytes", "gTeal", "pMetricTeal"),
        ("Ocupação", "74,79%", "gAmber", "pMetricAmber"),
        ("Folga", "264.336 bytes", "gBlue", "pMetric"),
        ("Pool LVGL", "96 KiB", "gTeal", "pMetricTeal"),
        ("DIRAM estática", "179.360 bytes", "gAmber", "pMetricAmber"),
    ]
    for i, (t, m, sty, ms) in enumerate(metrics):
        x = 1.35 + (i % 3) * 8.55
        y = 3.25 + (i // 3) * 4.55
        body += d.card(x, y, 7.8, 3.75, t, "valor da configuração final", metric=m, style=sty, metric_style=ms)
    body += d.card(5.65, 12.20, 16.4, 1.35, "Não medido", "heap e CPU por cenário · memória incremental por módulo", style="gRed")
    d.add_slide(info, body, "Fonte: build limpo, relatório size e mapa estático da versão final.")

    # 27 — apoio: evidências
    info = SlideInfo(27, "Ensaios e alcance das evidências", 0,
        "Responder com a categoria correta de evidência para cada resultado.",
        "Os instrumentos de consumo não formam cadeia metrológica rastreável. O alcance correto varia entre análise estática, teste funcional, comparação de plausibilidade e medição de corrente total.",
        "Retornar à pergunta da banca.", support=True)
    body = d.title("Ensaios e alcance das evidências", support=True)
    rows = [
        ("Arquitetura", "diff, build e remoção de sensores", "localização e contenção nas condições testadas"),
        ("PPG", "CSV + oxímetro/Amazfit", "coerência funcional; não validação clínica"),
        ("Temperatura", "um ponto + transientes", "estabilidade/resposta; não calibração"),
        ("LTR390", "ambientes + lanternas", "resposta qualitativa e saturação da configuração"),
        ("Bússola", "duas orientações", "resposta direcional; não exatidão geral"),
        ("Pedômetro", "um usuário, duas passagens", "resultado daquele cenário"),
        ("Energia", "corrente total em nove cenários", "sem decomposição e sem autonomia medida"),
    ]
    y = 3.05
    for i, (a, b, c) in enumerate(rows):
        body += d.rect(1.25, y, 25.5, 1.42, "gCard" if i % 2 == 0 else "gTeal")
        body += d.text(1.55, y + 0.20, 4.2, 0.85, ("pCardTitle", a))
        body += d.text(5.75, y + 0.20, 7.3, 0.85, ("pTiny", b))
        body += d.text(13.10, y + 0.20, 13.0, 0.85, ("pTiny", c))
        y += 1.55
    d.add_slide(info, body, "Fonte: caderno de ensaios e capítulo de Resultados e Discussão.")

    # 28 — apoio: referências
    info = SlideInfo(28, "Referências técnicas principais", 0,
        "Indicar as fontes primárias usadas nas decisões e conceitos.",
        "As referências completas e os acessos em 1º de julho de 2026 constam na monografia.",
        "Retornar à pergunta da banca.", support=True)
    body = d.title("Referências técnicas principais", support=True)
    refs_left = [
        "ESPRESSIF SYSTEMS. ESP32-C6 series datasheet. Version 1.5, 2025.",
        "ESPRESSIF SYSTEMS. ESP-IDF Programming Guide v5.5.1: I2C, SPI, RMT, NVS e watchdogs, 2025.",
        "FREERTOS. The FreeRTOS kernel book, 2026.",
        "NXP SEMICONDUCTORS. UM10204: I2C-bus specification and user manual. Rev. 7.0, 2021.",
        "NXP SEMICONDUCTORS. PCF8563: real-time clock/calendar. Rev. 11.1, 2026.",
    ]
    refs_right = [
        "ANALOG DEVICES. MAX30102 datasheet e Application Note 6409, 2018.",
        "ANALOG DEVICES. DS18B20 programmable resolution 1-Wire digital thermometer. Rev. 6, 2019.",
        "LITE-ON TECHNOLOGY. LTR-390UV-01 datasheet. Rev. C, 2024.",
        "TDK INVENSENSE. MPU-9250 product specification e register map, 2016–2017.",
        "AKM. AK8963: 3-axis electronic compass, 2013.",
        "RENAUDIN; AFZAL; LACHAPELLE. Complete triaxis magnetometer calibration, 2010.",
        "LVGL. Documentation v8.3: Objects e Drawing, 2023.",
    ]
    body += d.card(1.35, 3.10, 12.2, 10.2, "Plataforma, RTOS e interfaces", style="gBlue")
    body += d.text(1.85, 4.25, 11.2, 8.5, [("pRef", "• " + r) for r in refs_left])
    body += d.card(14.25, 3.10, 12.4, 10.2, "Sensores e processamento", style="gTeal")
    body += d.text(14.75, 4.25, 11.4, 8.5, [("pRef", "• " + r) for r in refs_right])
    d.add_slide(info, body, "Referências completas: Rascunho/TCC_COMPLETO.tex. Acesso em: 1 jul. 2026.")

    return d


def main() -> None:
    if not TEMPLATE.exists():
        raise SystemExit(f"Modelo oficial não encontrado: {TEMPLATE}")
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    deck = make_deck()
    deck.write_odp()
    deck.write_script()
    print(f"Gerado: {ODP_OUT}")
    print(f"Gerado: {SCRIPT_OUT}")
    print(f"Slides: {len(deck.pages)}; imagens incorporadas: {len(deck.assets)}")


if __name__ == "__main__":
    main()
