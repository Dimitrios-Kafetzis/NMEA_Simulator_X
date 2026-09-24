# SPDX-License-Identifier: GPL-3.0-only
"""Turn Doxygen's XML output into the Markdown pages of the C++ reference.

The MkDocs hook in this directory runs Doxygen and passes its XML directory to
`build_reference`, which returns every page as Markdown together with the navigation subtree
that goes under *Reference > C++ API*. Nothing is written to disk here; the hook hands the
pages to MkDocs as generated files, so they are rendered, themed and indexed for search like
the hand-written pages.

The reference has four parts, chosen by the path of the file that declares an entity:

- *Library API*: the public headers of `core` and `io` (`src/<lib>/include/`).
- *Library internals*: the implementation files and private headers of `core` and `io`.
- *Applications*: the desktop application and the command-line tool.
- *Test suite*: the tests, their fixtures and helpers.

Pages live under `reference/cpp/`: a namespace or class at its qualified name with `::`
replaced by `/`, a file at `files/<repository path>`, and a type from an anonymous namespace
under the page of the file that declares it. The output depends only on the XML and the
arguments, so two builds of the same commit produce the same pages.

The module can also be run on its own to inspect the Markdown it produces:

    python3 tools/doxygen/cppreference.py build/doxygen/xml /tmp/reference
"""

from __future__ import annotations

import dataclasses
import posixpath
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

#: Directory of the generated pages, relative to the documentation root.
ROOT = "reference/cpp"

#: Hand-written overview page of the reference, relative to the documentation root.
OVERVIEW_PAGE = "reference/api.md"


@dataclasses.dataclass(frozen=True)
class Part:
    """One of the four parts of the reference.

    Attributes:
        key: Identifier used in the model.
        title: Title in the navigation and on the index page.
        page: Path of the index page, relative to the documentation root.
        intro: Markdown paragraph at the top of the index page.
        file_prefix: Prefix removed from repository paths to form the navigation titles of
            the part's files.
    """

    key: str
    title: str
    page: str
    intro: str
    file_prefix: str


PARTS = (
    Part("library", "Library API", f"{ROOT}/library.md",
         "The public interface of the two libraries that other programs can link: "
         "`nmeasim::core`, the simulation engine and every encoder, with no Qt dependency, "
         "and `nmeasim::io`, the profiles, transports and runner built on Qt's non-GUI "
         "modules. Everything here is declared in the headers under `src/core/include/` and "
         "`src/io/include/`. Good starting points:\n\n"
         "- [`nmeasim::core::simulation::Simulation`](nmeasim/core/simulation/Simulation.md) "
         "advances a source and schedules the sentences.\n"
         "- [`nmeasim::core::nmea0183::SentenceRegistry`]"
         "(nmeasim/core/nmea0183/SentenceRegistry.md) lists every sentence the simulator can "
         "emit.\n"
         "- [`nmeasim::io::Profile`](nmeasim/io/Profile.md) is the persistent configuration and "
         "[`nmeasim::io::SimulationRunner`](nmeasim/io/SimulationRunner.md) runs it.",
         "src/"),
    Part("internals", "Library internals", f"{ROOT}/internals.md",
         "The implementation files and private headers of `core` and `io`, with the helpers "
         "that are local to them. None of this is part of the library interface.",
         "src/"),
    Part("applications", "Applications", f"{ROOT}/applications.md",
         "The desktop application (`nmeasim::app`, Qt Widgets) and the `nmeasim` "
         "command-line tool, both built on the libraries.",
         "src/"),
    Part("tests", "Test suite", f"{ROOT}/tests.md",
         "The Catch2 test suites of the libraries and the desktop application, with their "
         "fixtures and helpers. Each file page lists its test cases; their names are the "
         "specification the tests check.",
         "tests/"),
)
PART_BY_KEY = {part.key: part for part in PARTS}

#: Qt classes link to the Qt 6 documentation, whose page names follow the class names.
QT_DOCS = "https://doc.qt.io/qt-6/{}.html"

#: Section kinds of Doxygen compounds, in page order, with their headings.
SECTION_TITLES = {
    "public-type": "Public types",
    "public-func": "Public member functions",
    "public-static-func": "Public static member functions",
    "public-slot": "Public slots",
    "signal": "Signals",
    "public-attrib": "Public data members",
    "public-static-attrib": "Public static data members",
    "protected-type": "Protected types",
    "protected-func": "Protected member functions",
    "protected-static-func": "Protected static member functions",
    "protected-slot": "Protected slots",
    "protected-attrib": "Protected data members",
    "protected-static-attrib": "Protected static data members",
    "private-type": "Private types",
    "private-func": "Private member functions",
    "private-static-func": "Private static member functions",
    "private-slot": "Private slots",
    "private-attrib": "Private data members",
    "private-static-attrib": "Private static data members",
    "friend": "Friends",
    "related": "Related functions",
    "define": "Macros",
    "typedef": "Type aliases",
    "enum": "Enumerations",
    "func": "Functions",
    "var": "Variables",
}

#: Languages of `@code{.ext}` blocks, by the extension Doxygen reports.
CODE_LANGUAGES = {
    ".cpp": "cpp", ".hpp": "cpp", ".h": "cpp", ".py": "python", ".json": "json",
    ".sh": "bash", ".cmake": "cmake", ".xml": "xml", ".yaml": "yaml", ".yml": "yaml",
    ".txt": "text", ".unparsed": "text", ".md": "markdown", ".ini": "ini",
}

#: Characters of operator names and how they read in anchors.
OPERATOR_WORDS = {
    "=": "eq", "<": "lt", ">": "gt", "!": "not", "+": "plus", "-": "minus", "*": "star",
    "/": "slash", "%": "mod", "&": "amp", "|": "pipe", "^": "caret", "~": "tilde",
    "(": "call", ")": "", "[": "index", "]": "", ",": "comma",
}

#: Doxygen's XML elements for special characters.
ENTITIES = {
    "ndash": "–", "mdash": "—", "nonbreakablespace": " ", "sp": " ",
    "lsquo": "‘", "rsquo": "’", "ldquo": "“", "rdquo": "”",
    "deg": "°", "plusmn": "±", "times": "×", "divide": "÷",
    "copy": "©", "trade": "™", "reg": "®", "le": "≤", "ge": "≥",
    "ne": "≠", "hellip": "…", "larr": "←", "rarr": "→",
    "micro": "µ", "middot": "·", "szlig": "ß",
}

#: Admonition types of Doxygen's simple sections that render as boxes.
ADMONITIONS = {"note": "note", "warning": "warning", "attention": "warning",
               "remark": "info", "since": "info", "todo": "abstract"}

#: Titles of Doxygen's simple sections that render as labelled lists.
LABELLED_SECTIONS = {"pre": "Preconditions", "post": "Postconditions", "see": "See also",
                     "invariant": "Invariants", "author": "Author", "version": "Version",
                     "date": "Date"}

#: Ids of the fixed headings of a page, which member anchors must not take.
RESERVED_ANCHORS = ("summary", "namespaces", "types", "nested-types", "namespace-members",
                    "test-cases", "inherited-members")

#: Catch2 test cases, whose name may be split over adjacent string literals.
TEST_CASE_RE = re.compile(
    r'\bTEST_CASE\s*\(\s*((?:"(?:[^"\\]|\\.)*"\s*)+)(?:,\s*((?:"(?:[^"\\]|\\.)*"\s*)+))?\)')
STRING_LITERAL_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


def part_of_path(path: str) -> str | None:
    """Return the key of the part that a repository file belongs to.

    Args:
        path: Repository-relative path with forward slashes.

    Returns:
        The part key, or None for a file outside the documented directories.
    """
    if path.startswith(("src/core/include/", "src/io/include/")) or path == "src/namespaces.dox":
        return "library"
    if path.startswith(("src/core/", "src/io/")):
        return "internals"
    if path.startswith(("src/app/", "src/cli/")):
        return "applications"
    if path.startswith("tests/"):
        return "tests"
    return None


def md_escape(text: str) -> str:
    """Escape text so that Markdown renders it literally.

    Args:
        text: Plain text from a comment.

    Returns:
        The text with Markdown and HTML metacharacters escaped.
    """
    text = text.replace("\\", "\\\\").replace("&", "&amp;")
    text = text.replace("<", "&lt;").replace(">", "&gt;")
    return re.sub(r"([`*_\[\]{}|#])", r"\\\1", text)


def code_span(text: str) -> str:
    """Return `text` as an inline code span, whatever backticks it contains.

    Args:
        text: Code to show verbatim.

    Returns:
        The Markdown code span.
    """
    text = text.strip()
    if not text:
        return ""
    if "`" not in text:
        return f"`{text}`"
    return f"`` {text} ``"


def tidy_type(text: str) -> str:
    """Normalise the spacing Doxygen gives C++ types to the project's style.

    Doxygen writes `const QString &text`; the code base writes `const QString& text`.

    Args:
        text: A type or declaration as Doxygen prints it.

    Returns:
        The same text with `&`, `&&` and `*` attached to the type.
    """
    text = re.sub(r"\s+", " ", text).strip()
    text = re.sub(r"\s+([&*]+)(?=[\w.]|$|\s*[,)])", r"\1 ", text)
    text = re.sub(r"< ", "<", text)
    text = re.sub(r" >", ">", text)
    return re.sub(r"\s+", " ", text).strip()


def element_text(element: ET.Element | None) -> str:
    """Return the text content of an XML element, with `<sp/>` as spaces.

    Args:
        element: The element, or None.

    Returns:
        All text below the element, whitespace preserved.
    """
    if element is None:
        return ""
    parts = [element.text or ""]
    for child in element:
        parts.append(" " if child.tag == "sp" else element_text(child))
        parts.append(child.tail or "")
    return "".join(parts)


def anchor_slug(name: str) -> str:
    """Return an HTML id for a member name, readable for operators and destructors.

    Args:
        name: The unqualified member name, such as `advance` or `operator==`.

    Returns:
        A string of letters, digits, underscores and hyphens.
    """
    if name.startswith("~"):
        return "destructor-" + anchor_slug(name[1:])
    if name.startswith("operator") and not re.fullmatch(r"operator\w*", name):
        symbols = name[len("operator"):].strip()
        words = [OPERATOR_WORDS.get(char, "") for char in symbols]
        slug = "-".join(word for word in words if word)
        return f"operator-{slug}" if slug else "operator"
    slug = re.sub(r"[^A-Za-z0-9_]+", "-", name).strip("-")
    return slug or "member"


def relative_link(from_page: str, to_page: str, anchor: str | None = None) -> str:
    """Return the relative Markdown link from one generated page to another.

    Args:
        from_page: Path of the page that contains the link.
        to_page: Path of the page linked to.
        anchor: Optional id on the target page.

    Returns:
        A link that MkDocs resolves and validates, such as `../geo/Position.md#latitude_deg`.
    """
    if from_page == to_page:
        return f"#{anchor}" if anchor else "#"
    link = posixpath.relpath(to_page, posixpath.dirname(from_page))
    return f"{link}#{anchor}" if anchor else link


@dataclasses.dataclass
class Compound:
    """A namespace, class, struct, union or file that gets a page of its own.

    Attributes:
        refid: Doxygen's identifier of the compound.
        kind: `namespace`, `class`, `struct`, `union` or `file`.
        name: Qualified name, or the repository path for a file.
        element: The `compounddef` element.
        page: Path of the page, relative to the documentation root.
        part: Key of the part the page belongs to.
        title: Short title for the heading and the navigation.
        file: Repository path of the file that declares the compound.
        members: Members documented in full on this page, in page order.
    """

    refid: str
    kind: str
    name: str
    element: ET.Element
    page: str
    part: str
    title: str
    file: str
    members: list[Member] = dataclasses.field(default_factory=list)

    @property
    def brief(self) -> ET.Element | None:
        """The `briefdescription` element."""
        return self.element.find("briefdescription")


@dataclasses.dataclass
class Member:
    """A function, variable, type alias, enumeration, signal, slot, friend or macro.

    Attributes:
        refid: Doxygen's identifier of the member.
        element: The `memberdef` element.
        section: Kind of the `sectiondef` that lists it, such as `public-func`.
        page: Path of the page that documents it in full.
        anchor: Id of its heading on that page.
    """

    refid: str
    element: ET.Element
    section: str
    page: str
    anchor: str

    @property
    def name(self) -> str:
        """The unqualified name."""
        return self.element.findtext("name") or ""

    @property
    def kind(self) -> str:
        """Doxygen's member kind, such as `function` or `signal`."""
        return self.element.get("kind", "")


class Model:
    """Every page of the reference and the link target of every documented entity.

    Args:
        xml_dir: Doxygen's XML output directory.
    """

    def __init__(self, xml_dir: Path) -> None:
        """Load the XML and assign every compound and member to a page."""
        self.xml_dir = xml_dir
        self.compounds: dict[str, Compound] = {}
        self.members: dict[str, Member] = {}
        #: Member ids of enumerators, which link to their enumeration.
        self.aliases: dict[str, str] = {}
        #: File compounds, by repository path.
        self.files: dict[str, Compound] = {}
        self._load()

    def _definitions(self) -> list[ET.Element]:
        """Return the `compounddef` elements of every compound in the index, in index order."""
        index = ET.parse(self.xml_dir / "index.xml").getroot()
        definitions = []
        for entry in index.findall("compound"):
            if entry.get("kind") in ("dir", "page", "example", "group", "concept"):
                continue
            tree = ET.parse(self.xml_dir / f"{entry.get('refid')}.xml")
            definitions.extend(tree.getroot().findall("compounddef"))
        return definitions

    def _load(self) -> None:
        """Build the compounds, then the members, then the anchors."""
        definitions = self._definitions()
        real_classes = self._real_classes(definitions)
        files = [d for d in definitions if d.get("kind") == "file"]
        for definition in files:
            path = definition.find("location").get("file", "")
            part = part_of_path(path)
            # `.dox` files hold namespace comments only; the namespaces have pages of their own.
            if part is None or path.endswith(".dox"):
                continue
            compound = Compound(definition.get("id"), "file", path, definition,
                                f"{ROOT}/files/{path}.md", part, posixpath.basename(path), path)
            self.compounds[compound.refid] = compound
            self.files[path] = compound

        for definition in definitions:
            kind = definition.get("kind")
            name = definition.findtext("compoundname") or ""
            location = definition.find("location")
            path = location.get("file", "") if location is not None else ""
            if kind == "namespace":
                if not name.startswith("nmeasim") or "anonymous_namespace{" in name:
                    continue
                page = f"{ROOT}/{name.replace('::', '/')}/index.md"
                self.compounds[definition.get("id")] = Compound(
                    definition.get("id"), kind, name, definition, page,
                    self._namespace_part(name, path), name, path)
            elif kind in ("class", "struct", "union"):
                if definition.get("id") not in real_classes or path not in self.files:
                    continue
                page, title = self._class_page(name, path)
                self.compounds[definition.get("id")] = Compound(
                    definition.get("id"), kind, name, definition, page,
                    self.files[path].part, title, path)

        self._check_unique_pages()
        for compound in self.ordered(("namespace", "class", "struct", "union")):
            self._collect_members(compound, compound.element)
        # Members of anonymous namespaces and of the global scope are documented on the page
        # of the file that declares them.
        for definition in definitions:
            name = definition.findtext("compoundname") or ""
            if definition.get("kind") == "namespace" and "anonymous_namespace{" in name:
                path = definition.find("location").get("file", "")
                if path in self.files:
                    self._collect_members(self.files[path], definition)
        for compound in self.ordered(("file",)):
            self._collect_members(compound, compound.element, global_only=True)
        for compound in self.compounds.values():
            self._assign_anchors(compound)

    @staticmethod
    def _real_classes(definitions: list[ET.Element]) -> set[str]:
        """Return the ids of the classes that are declared where Doxygen says they are.

        A `using a::B;` declaration makes Doxygen report a copy of `B` in the scope of the
        declaration, with the location of the original. The original is the one that the
        file at that location lists as its own, or that a real class lists as nested.
        """
        listed: dict[str, set[str]] = {}
        classes = []
        for definition in definitions:
            inner = {c.get("refid", "") for c in definition.findall("innerclass")}
            if definition.get("kind") == "file":
                path = definition.find("location").get("file", "")
                listed.setdefault(path, set()).update(inner)
            elif definition.get("kind") in ("class", "struct", "union"):
                classes.append(definition)
        real: set[str] = set()
        nested: set[str] = set()
        for definition in sorted(classes, key=lambda d: len(d.findtext("compoundname") or "")):
            refid = definition.get("id", "")
            path = definition.find("location").get("file", "")
            if refid in listed.get(path, set()) or refid in nested:
                real.add(refid)
                nested.update(c.get("refid", "") for c in definition.findall("innerclass"))
        return real

    def _namespace_part(self, name: str, path: str) -> str:
        """Return the part of a namespace page, from its name and where it is documented."""
        if name.startswith("nmeasim::app"):
            return "applications"
        if name.startswith("nmeasim::test"):
            return "tests"
        return "internals" if part_of_path(path) == "internals" else "library"

    def _class_page(self, name: str, path: str) -> tuple[str, str]:
        """Return the page path and the title of a class, struct or union page."""
        if "anonymous_namespace{" in name:
            local = name.rsplit("}::", 1)[1]
            return f"{ROOT}/files/{path}/{local.replace('::', '/')}.md", local
        parts = name.split("::")
        # Nested types are titled with their enclosing class, `Outer::Inner`.
        scope = []
        for part in parts[:-1]:
            if scope or (part and part[0].isupper()):
                scope.append(part)
        return f"{ROOT}/{name.replace('::', '/')}.md", "::".join(scope + [parts[-1]])

    def _check_unique_pages(self) -> None:
        """Fail when two pages would share a path on a case-insensitive file system."""
        seen: dict[str, str] = {}
        for compound in self.compounds.values():
            key = compound.page.lower().removesuffix("/index.md").removesuffix(".md")
            if key in seen:
                raise ValueError(f"{compound.name} and {seen[key]} would share the page {key}")
            seen[key] = compound.name

    def ordered(self, kinds: tuple[str, ...]) -> list[Compound]:
        """Return the compounds of the given kinds, sorted by name.

        Args:
            kinds: Compound kinds such as `namespace` and `struct`.
        """
        return sorted((c for c in self.compounds.values() if c.kind in kinds),
                      key=lambda c: c.name)

    def _collect_members(self, compound: Compound, definition: ET.Element,
                         global_only: bool = False) -> None:
        """Assign the members listed by a `compounddef` to the page of `compound`.

        Args:
            compound: The page that documents the members.
            definition: The `compounddef` whose sections list them.
            global_only: For a file, only take members of the global scope; namespace
                members listed there are documented on their namespace's page.
        """
        for section in definition.findall("sectiondef"):
            for element in section.findall("memberdef"):
                refid = element.get("id", "")
                if refid in self.members:
                    continue
                if global_only and not refid.startswith(definition.get("id", "") + "_1"):
                    continue
                member = Member(refid, element, section.get("kind", ""), compound.page, "")
                self.members[refid] = member
                compound.members.append(member)
                for value in element.findall("enumvalue"):
                    self.aliases[value.get("id", "")] = refid

    def _assign_anchors(self, compound: Compound) -> None:
        """Give every member of a page a unique anchor, in page order."""
        # Ids the page writer gives its own headings; a member of the same name gets a suffix.
        used: dict[str, int] = {anchor: 1 for anchor in RESERVED_ANCHORS}
        for member in sorted_members(compound):
            slug = anchor_slug(member.name)
            count = used.get(slug, 0) + 1
            used[slug] = count
            member.anchor = slug if count == 1 else f"{slug}-{count}"

    def target(self, refid: str, from_page: str) -> str | None:
        """Return the link from a page to the entity with a Doxygen id, if it has a page.

        Args:
            refid: Doxygen's identifier of a compound, member or enumerator.
            from_page: Path of the page that contains the link.

        Returns:
            A relative link, or None for an entity outside the reference and for the page's
            own subject.
        """
        refid = self.aliases.get(refid, refid)
        if refid in self.compounds:
            page = self.compounds[refid].page
            # A page's mention of its own subject stays plain text.
            return None if page == from_page else relative_link(from_page, page)
        if refid in self.members:
            member = self.members[refid]
            return relative_link(from_page, member.page, member.anchor)
        return None


def parameter_names(element: ET.Element) -> set[str]:
    """Return the parameter names of a member declaration."""
    return {param.findtext("declname") or "" for param in element.findall("param")} - {""}


def section_order(kind: str) -> int:
    """Return the position of a section kind on a page; unknown kinds go last."""
    keys = list(SECTION_TITLES)
    return keys.index(kind) if kind in keys else len(keys)


def sorted_members(compound: Compound) -> list[Member]:
    """Return the members of a page grouped by section, in declaration order within one."""
    return sorted(compound.members, key=lambda m: section_order(m.section))


class Writer:
    """Renders Doxygen descriptions and declarations as Markdown for one page.

    Args:
        model: The reference model, for links.
        page: Path of the page being written, for relative links.
    """

    def __init__(self, model: Model, page: str) -> None:
        """Start a writer for `page`."""
        self.model = model
        self.page = page
        #: Parameter names of the member being written; code spans naming them stay unlinked
        #: even when a member of the same name exists.
        self.parameters: set[str] = set()

    # Inline content ------------------------------------------------------------------------

    def inline(self, element: ET.Element | None, code: bool = False) -> str:
        """Render the inline content of a description element.

        Args:
            element: A `para` or any element with mixed content.
            code: Whether the content sits inside a code span, which suppresses escaping.

        Returns:
            Markdown text on one line.
        """
        if element is None:
            return ""
        parts = [self._text(element.text or "", code)]
        for child in element:
            parts.append(self._inline_element(child, code))
            parts.append(self._text(child.tail or "", code))
        return "".join(parts)

    def _text(self, text: str, code: bool) -> str:
        """Collapse whitespace and escape unless inside code."""
        text = re.sub(r"\s+", " ", text)
        return text if code else md_escape(text)

    def _inline_element(self, element: ET.Element, code: bool) -> str:
        """Render one inline element; block elements are handled by `blocks`."""
        tag = element.tag
        if tag in ENTITIES:
            return ENTITIES[tag]
        if tag == "ref":
            text = self.inline(element, code=True)
            if code:
                return text
            link = (None if text in self.parameters
                    else self.model.target(element.get("refid", ""), self.page))
            return f"[{code_span(text)}]({link})" if link else code_span(text)
        if tag == "computeroutput":
            text = element_text(element).strip()
            refs = element.findall("ref")
            link = (self.model.target(refs[0].get("refid", ""), self.page)
                    if len(refs) == 1 and text not in self.parameters else None)
            return f"[{code_span(text)}]({link})" if link else code_span(text)
        if tag == "emphasis":
            return f"*{self.inline(element, code).strip()}*"
        if tag == "bold":
            return f"**{self.inline(element, code).strip()}**"
        if tag == "ulink":
            text = self.inline(element, code).strip() or element.get("url", "")
            return f"[{text}]({element.get('url', '')})"
        if tag == "linebreak":
            return "<br>"
        if tag in ("superscript", "subscript"):
            html_tag = "sup" if tag == "superscript" else "sub"
            return f"<{html_tag}>{self.inline(element, code)}</{html_tag}>"
        if tag == "formula":
            return code_span(element_text(element).strip("$ "))
        if tag in ("anchor", "image", "htmlonly", "latexonly", "rtfonly", "manonly",
                   "xmlonly", "docbookonly", "indexentry"):
            return ""
        return self.inline(element, code)

    # Block content -------------------------------------------------------------------------

    def description(self, element: ET.Element | None) -> Description:
        """Split a `briefdescription` or `detaileddescription` into blocks and sections.

        Args:
            element: The description element.

        Returns:
            The paragraphs, lists and code blocks, and the special sections (parameters,
            return value, exceptions, notes) found inside them.
        """
        result = Description()
        if element is not None:
            for child in element:
                self._block(child, result)
        return result

    def _block(self, element: ET.Element, result: Description) -> None:
        """Render a block-level element into `result`."""
        tag = element.tag
        if tag == "para":
            self._para(element, result)
        elif tag in ("sect1", "sect2", "sect3", "sect4"):
            title = self.inline(element.find("title")).strip()
            if title:
                result.blocks.append(f"**{title}**")
            for child in element:
                if child.tag != "title":
                    self._block(child, result)
        elif tag == "internal":
            for child in element:
                self._block(child, result)
        else:
            self._para(element, result)

    def _para(self, element: ET.Element, result: Description) -> None:
        """Render a paragraph, which may contain lists, code and special sections."""
        text = [self._text(element.text or "", False)]

        def flush() -> None:
            paragraph = "".join(text).strip()
            if paragraph:
                result.blocks.append(paragraph)
            text.clear()

        for child in element:
            tag = child.tag
            if tag in ("itemizedlist", "orderedlist"):
                flush()
                result.blocks.append(self._list(child))
            elif tag in ("programlisting", "verbatim", "preformatted"):
                flush()
                result.blocks.append(self._code_block(child))
            elif tag == "simplesect":
                flush()
                self._simplesect(child, result)
            elif tag == "parameterlist":
                flush()
                self._parameterlist(child, result)
            elif tag == "xrefsect":
                flush()
                title = element_text(child.find("xreftitle")).strip()
                body = self.description(child.find("xrefdescription"))
                result.admonitions.append(("warning" if title == "Deprecated" else "abstract",
                                           title, body.markdown()))
            elif tag == "table":
                flush()
                result.blocks.append(self._table(child))
            elif tag == "heading":
                flush()
                result.blocks.append(f"**{self.inline(child).strip()}**")
            elif tag == "blockquote":
                flush()
                quoted = self.description(child).markdown()
                result.blocks.append("\n".join(f"> {line}" if line else ">"
                                               for line in quoted.splitlines()))
            elif tag in ("parblock", "details"):
                flush()
                for grandchild in child:
                    self._block(grandchild, result)
            elif tag == "hruler":
                flush()
                result.blocks.append("---")
            else:
                text.append(self._inline_element(child, False))
            text.append(self._text(child.tail or "", False))
        flush()

    def _list(self, element: ET.Element) -> str:
        """Render an itemized or ordered list, indenting nested blocks by four spaces."""
        ordered = element.tag == "orderedlist"
        items = []
        for number, item in enumerate(element.findall("listitem"), start=1):
            body = self.description(item)
            blocks = body.blocks + [box for box in body.admonition_blocks()]
            marker = f"{number}." if ordered else "-"
            first, *rest = blocks or [""]
            lines = [f"{marker} {first}"]
            for block in rest:
                lines.append("")
                lines.extend(f"    {line}" if line else "" for line in block.splitlines())
            items.append("\n".join(lines))
        return "\n".join(items)

    def _code_block(self, element: ET.Element) -> str:
        """Render a code listing or verbatim block as a fenced code block."""
        if element.tag == "programlisting":
            language = CODE_LANGUAGES.get(element.get("filename", ".cpp"), "text")
            lines = [element_text(line).rstrip() for line in element.findall("codeline")]
        else:
            language = "text"
            lines = element_text(element).strip("\n").splitlines()
        return "\n".join([f"```{language}", *lines, "```"])

    def _table(self, element: ET.Element) -> str:
        """Render a Markdown table from a comment as a Markdown table."""
        rows = []
        for row in element.findall("row"):
            cells = [self.description(entry).inline() for entry in row.findall("entry")]
            rows.append("| " + " | ".join(cells) + " |")
        if not rows:
            return ""
        columns = rows[0].count(" | ") + 1
        rows.insert(1, "| " + " | ".join(["---"] * columns) + " |")
        return "\n".join(rows)

    def _simplesect(self, element: ET.Element, result: Description) -> None:
        """File a `@return`, `@see`, `@note` or similar section under its kind."""
        kind = element.get("kind", "")
        body = self.description(element)
        if kind == "return":
            result.returns.append(body.markdown())
        elif kind in ADMONITIONS:
            result.admonitions.append((ADMONITIONS[kind], kind.capitalize(), body.markdown()))
        elif kind in LABELLED_SECTIONS:
            result.labelled.setdefault(LABELLED_SECTIONS[kind], []).append(body.inline())
        elif kind == "par":
            title = self.inline(element.find("title")).strip()
            result.blocks.append(f"**{title}**" if title else "")
            result.blocks.extend(body.blocks)
        else:
            result.blocks.extend(body.blocks)

    def _parameterlist(self, element: ET.Element, result: Description) -> None:
        """File the entries of a `@param`, `@tparam`, `@throws` or `@retval` list."""
        kind = element.get("kind", "param")
        for item in element.findall("parameteritem"):
            names = []
            for name_list in item.findall("parameternamelist"):
                for name in name_list.findall("parametername"):
                    direction = name.get("direction")
                    label = self.inline(name, code=True).strip()
                    names.append((label, direction))
            body = self.description(item.find("parameterdescription")).inline()
            for label, direction in names:
                if kind == "param":
                    result.params[label] = (direction, body)
                elif kind == "templateparam":
                    result.tparams[label] = body
                elif kind == "exception":
                    link = None
                    ref = item.find("parameternamelist/parametername/ref")
                    if ref is not None:
                        link = self.model.target(ref.get("refid", ""), self.page)
                    result.exceptions.append((label, link, body))
                elif kind == "retval":
                    result.retvals.append((label, body))

    # Declarations --------------------------------------------------------------------------

    def linked_type(self, element: ET.Element | None) -> str:
        """Render a `type` element as code, linked when it names a documented entity.

        Qt classes link to the Qt documentation.
        """
        if element is None:
            return ""
        text = tidy_type(element_text(element))
        if not text:
            return ""
        refs = [ref for ref in element.findall("ref")
                if self.model.target(ref.get("refid", ""), self.page)]
        if len(refs) == 1:
            return f"[{code_span(text)}]({self.model.target(refs[0].get('refid', ''), self.page)})"
        if len(refs) > 1:
            links = ", ".join(f"[{code_span(element_text(ref))}]"
                              f"({self.model.target(ref.get('refid', ''), self.page)})"
                              for ref in refs)
            return f"{code_span(text)} ({links})"
        qt_class = re.fullmatch(r"(?:const )?(Q[A-Z]\w+)[&*]?", text)
        if qt_class:
            return f"[{code_span(text)}]({QT_DOCS.format(qt_class.group(1).lower())})"
        return code_span(text)


@dataclasses.dataclass
class Description:
    """A description split into running text and the special sections Doxygen marks up.

    Attributes:
        blocks: Paragraphs, lists, tables and code blocks, in order.
        params: Parameter name to direction (`in`, `out`, `inout` or None) and description.
        tparams: Template parameter name to description.
        returns: Descriptions from `@return`.
        retvals: Value and description pairs from `@retval`.
        exceptions: Exception type, link or None, and description from `@throws`.
        labelled: Titled lists such as preconditions and "See also".
        admonitions: Admonition type, title and body from `@note`, `@warning` and similar.
    """

    blocks: list[str] = dataclasses.field(default_factory=list)
    params: dict[str, tuple[str | None, str]] = dataclasses.field(default_factory=dict)
    tparams: dict[str, str] = dataclasses.field(default_factory=dict)
    returns: list[str] = dataclasses.field(default_factory=list)
    retvals: list[tuple[str, str]] = dataclasses.field(default_factory=list)
    exceptions: list[tuple[str, str | None, str]] = dataclasses.field(default_factory=list)
    labelled: dict[str, list[str]] = dataclasses.field(default_factory=dict)
    admonitions: list[tuple[str, str, str]] = dataclasses.field(default_factory=list)

    def markdown(self) -> str:
        """Return the running text and admonitions as Markdown blocks."""
        return "\n\n".join(self.blocks + self.admonition_blocks())

    def inline(self) -> str:
        """Return the running text on one line, for a table cell."""
        return " ".join(block.replace("\n", " ") for block in self.blocks).strip()

    def admonition_blocks(self) -> list[str]:
        """Return the admonitions as Material admonition blocks."""
        boxes = []
        for kind, title, body in self.admonitions:
            indented = "\n".join(f"    {line}" if line else "" for line in body.splitlines())
            boxes.append(f'!!! {kind} "{title}"\n\n{indented}')
        return boxes


class PageWriter(Writer):
    """Writes the Markdown of one compound page.

    Args:
        model: The reference model.
        compound: The compound the page documents.
        source: Callable returning the GitHub URL of a repository file and line.
        root: Repository root, for reading the test cases of test files.
    """

    def __init__(self, model: Model, compound: Compound, source, root: Path) -> None:
        """Start the page of `compound`."""
        super().__init__(model, compound.page)
        self.compound = compound
        self.source = source
        self.root = root
        self.lines: list[str] = []

    def emit_block(self, text: str) -> None:
        """Append a Markdown block followed by an empty line."""
        if text:
            self.lines.extend([text, ""])

    def part_link(self) -> str:
        """Return a link to the index page of the compound's part."""
        part = PART_BY_KEY[self.compound.part]
        return f"[{part.title}]({relative_link(self.page, part.page)})"

    def file_link(self, path: str, line: str | None = None) -> str:
        """Return a link to the page of a documented file, with the line on GitHub."""
        file = self.model.files.get(path)
        label = code_span(path)
        text = f"[{label}]({relative_link(self.page, file.page)})" if file else label
        if line:
            text += f" ([line {line}]({self.source(path, line)}))"
        return text

    def write(self) -> str:
        """Return the page as Markdown."""
        compound = self.compound
        kind_label = {"struct": "struct", "class": "class", "union": "union",
                      "namespace": "namespace", "file": "file"}[compound.kind]
        self.emit_block(f"# {md_escape(compound.title)}")
        shown = compound.name.replace("anonymous_namespace", "(anonymous namespace)")
        self.emit_block(f"*{kind_label}* {code_span(shown)} · {self.part_link()}")
        brief = self.description(compound.brief)
        self.emit_block(brief.markdown())
        if compound.kind in ("class", "struct", "union"):
            self._class_header()
        elif compound.kind == "file":
            self._file_header()
        detail = self.description(compound.element.find("detaileddescription"))
        self._description_body(detail)
        if compound.kind == "file":
            self._test_cases()
        self._inner_tables()
        self._summary()
        self._details()
        if compound.kind in ("class", "struct", "union"):
            self._inherited()
        # The wrapper lets the site stylesheet style reference pages only.
        title, body = self.lines[0], "\n".join(self.lines[2:]).rstrip()
        return f'{title}\n\n<div class="cpp-reference" markdown>\n\n{body}\n\n</div>\n'

    # Headers of the page -------------------------------------------------------------------

    def _class_header(self) -> None:
        """Write the include line, the bases, the derived classes and the source links."""
        element = self.compound.element
        include = element.find("includes")
        if include is not None and self.compound.part == "library":
            self.emit_block(f"```cpp\n#include <{element_text(include).strip()}>\n```")
        bases = []
        for base in element.findall("basecompoundref"):
            name = tidy_type(element_text(base))
            link = self.model.target(base.get("refid", ""), self.page)
            if link is None and re.fullmatch(r"Q[A-Z]\w+", name):
                link = QT_DOCS.format(name.lower())
            prot = base.get("prot", "public")
            label = f"[{code_span(name)}]({link})" if link else code_span(name)
            bases.append(label if prot == "public" else f"{label} ({prot})")
        if bases:
            self.emit_block("**Inherits** " + ", ".join(bases))
        derived = []
        for child in element.findall("derivedcompoundref"):
            link = self.model.target(child.get("refid", ""), self.page)
            name = tidy_type(element_text(child))
            derived.append(f"[{code_span(name)}]({link})" if link else code_span(name))
        if derived:
            self.emit_block("**Inherited by** " + ", ".join(derived))
        location = element.find("location")
        self.emit_block("**Declared in** " + self.file_link(location.get("file", ""),
                                                            location.get("line")))

    def _file_header(self) -> None:
        """Write the include line of a public header and the link to the source."""
        path = self.compound.name
        if self.compound.part == "library" and path.endswith((".hpp", ".hpp.in")):
            include = path.split("/include/", 1)[1].removesuffix(".in")
            self.emit_block(f"```cpp\n#include <{include}>\n```")
        self.emit_block(f"**Source** [{code_span(path)}]({self.source(path, None)})")

    def _description_body(self, detail: Description) -> None:
        """Write the detailed description of the compound itself."""
        if detail.blocks or detail.admonitions:
            self.emit_block(detail.markdown())
        self._labelled(detail)
        if detail.tparams:
            self._tparams(detail)

    def _test_cases(self) -> None:
        """List the Catch2 test cases of a test file, in source order."""
        if self.compound.part != "tests":
            return
        path = self.root / self.compound.name
        if not path.is_file():
            return
        text = path.read_text(encoding="utf-8")
        rows = []
        for match in TEST_CASE_RE.finditer(text):
            name = "".join(STRING_LITERAL_RE.findall(match.group(1)))
            tags = "".join(STRING_LITERAL_RE.findall(match.group(2) or ""))
            name = name.replace('\\"', '"').replace("\\\\", "\\")
            line = str(text.count("\n", 0, match.start()) + 1)
            rows.append(f"| [{md_escape(name)}]({self.source(self.compound.name, line)}) "
                        f"| {code_span(tags)} |")
        if rows:
            self.emit_block(f"## Test cases {{#test-cases}}\n\n{len(rows)} test cases, in "
                            "source order.\n\n| Test case | Tags |\n| --- | --- |\n"
                            + "\n".join(rows))

    # Tables of contained entities -----------------------------------------------------------

    def _inner_tables(self) -> None:
        """Write the tables of nested namespaces and of the classes the compound contains."""
        element = self.compound.element
        namespaces = []
        for inner in element.findall("innernamespace"):
            target = self.model.compounds.get(inner.get("refid", ""))
            if target is not None:
                namespaces.append(target)
        if self.compound.kind == "file":
            # A file lists every enclosing namespace; name only the innermost ones.
            names = {c.name for c in namespaces}
            namespaces = [c for c in namespaces
                          if not any(n.startswith(c.name + "::") for n in names)]
        if namespaces:
            self.emit_block("## Namespaces {#namespaces}\n\n| Namespace | Description |\n"
                            "| --- | --- |\n" + "\n".join(self._compound_row(c)
                                                          for c in namespaces))
        classes = [self.model.compounds[refid] for refid in self._inner_class_ids()]
        if classes:
            heading = "Nested types" if self.compound.kind in ("class", "struct") else "Types"
            anchor = "nested-types" if heading == "Nested types" else "types"
            self.emit_block(f"## {heading} {{#{anchor}}}\n\n| Type | Description |\n"
                            "| --- | --- |\n" + "\n".join(self._compound_row(c) for c in classes))
        if self.compound.kind == "file":
            self._file_members()

    def _inner_class_ids(self) -> list[str]:
        """Return the ids of the classes listed by the compound, including anonymous ones."""
        ids = [inner.get("refid", "") for inner in self.compound.element.findall("innerclass")]
        return [refid for refid in ids if refid in self.model.compounds]

    def _compound_row(self, compound: Compound) -> str:
        """Return a table row linking to a compound, with its brief description."""
        link = relative_link(self.page, compound.page)
        name = compound.title if compound.kind != "namespace" else compound.name
        return f"| [{code_span(name)}]({link}) | {self.description(compound.brief).inline()} |"

    def _file_members(self) -> None:
        """List the namespace members a file declares or defines, linking to their pages."""
        rows = []
        for section in self.compound.element.findall("sectiondef"):
            for element in section.findall("memberdef"):
                member = self.model.members.get(element.get("id", ""))
                if member is None or member.page == self.page:
                    continue
                link = relative_link(self.page, member.page, member.anchor)
                qualified = element.findtext("qualifiedname") or member.name
                brief = self.description(member.element.find("briefdescription")).inline()
                rows.append(f"| [{code_span(qualified)}]({link}) | {brief} |")
        if rows:
            self.emit_block("## Namespace members {#namespace-members}\n\n"
                            "Declared or defined in this file and documented on the page of "
                            "their namespace.\n\n| Member | Description |\n| --- | --- |\n"
                            + "\n".join(rows))

    # Members ---------------------------------------------------------------------------------

    def _groups(self) -> list[tuple[str, list[Member]]]:
        """Return the page's members grouped by section, in page order."""
        groups: dict[str, list[Member]] = {}
        for member in sorted_members(self.compound):
            groups.setdefault(member.section, []).append(member)
        return list(groups.items())

    def _section_title(self, section: str) -> str:
        """Return the heading of a member section on this page."""
        title = SECTION_TITLES.get(section, section.replace("-", " ").capitalize())
        if self.compound.kind == "file":
            return f"File-local {title[0].lower()}{title[1:]}" if section != "define" else title
        return title

    def _summary(self) -> None:
        """Write one summary table per member section."""
        groups = self._groups()
        if not groups:
            return
        self.emit_block("## Summary {#summary}")
        for section, members in groups:
            rows = []
            for member in members:
                name = member.name + ("()" if member.kind in ("function", "signal", "slot")
                                      else "")
                self.parameters = parameter_names(member.element)
                brief = self.description(member.element.find("briefdescription")).inline()
                self.parameters = set()
                rows.append(f"| [{code_span(name)}](#{member.anchor}) | {brief} |")
            self.emit_block(f"**{self._section_title(section)}**\n\n| Name | Description |\n"
                            "| --- | --- |\n" + "\n".join(rows))

    def _details(self) -> None:
        """Write every member in full, one section heading per member section."""
        for section, members in self._groups():
            slug = "section-" + section
            self.emit_block(f"## {self._section_title(section)} {{#{slug}}}")
            for member in members:
                self._member(member)

    def _member(self, member: Member) -> None:
        """Write the heading, signature and description of one member."""
        element = member.element
        self.parameters = parameter_names(element)
        suffix = "()" if member.kind in ("function", "signal", "slot") else ""
        self.emit_block(f"### {md_escape(member.name)}{suffix} {{#{member.anchor}}}")
        self.emit_block(f"```cpp\n{self.signature(element)}\n```")
        brief = self.description(element.find("briefdescription"))
        detail = self.description(element.find("detaileddescription"))
        self.emit_block(brief.markdown())
        if detail.blocks:
            self.emit_block("\n\n".join(detail.blocks))
        if member.kind == "enum":
            self._enumerators(element)
        self._parameters(element, detail)
        self._tparams(detail)
        self._returns(element, detail)
        self._exceptions(detail)
        self._labelled(detail)
        for box in detail.admonition_blocks():
            self.emit_block(box)
        self._member_facts(member)
        self.parameters = set()

    def signature(self, element: ET.Element) -> str:
        """Return the C++ declaration of a member as the code base would write it."""
        kind = element.get("kind", "")
        name = element.findtext("name") or ""
        template = self._template(element)
        if kind == "enum":
            strong = " class" if element.get("strong") == "yes" else ""
            underlying = element.findtext("type") or ""
            return (f"{template}enum{strong} {name}"
                    + (f" : {tidy_type(underlying)}" if underlying.strip() else ""))
        if kind == "typedef":
            definition = re.sub(r"\s+", " ", element.findtext("definition") or "").strip()
            if definition.startswith("using"):
                return f"{template}using {name} = {tidy_type(element_text(element.find('type')))}"
            return (f"typedef {tidy_type(element_text(element.find('type')))} {name}"
                    f"{element.findtext('argsstring') or ''}")
        if kind == "define":
            params = [element_text(p.find("defname")) for p in element.findall("param")]
            args = f"({', '.join(params)})" if element.find("param") is not None else ""
            value = element_text(element.find("initializer")).strip()
            return f"#define {name}{args}" + (f" {value}" if value else "")
        prefix = []
        type_text = tidy_type(element_text(element.find("type")))
        if element.get("static") == "yes" and "static" not in type_text.split():
            prefix.append("static")
        if element.get("explicit") == "yes":
            prefix.append("explicit")
        if element.get("virt") in ("virtual", "pure-virtual") and "override" not in (
                element.findtext("argsstring") or ""):
            prefix.append("virtual")
        if kind == "friend":
            prefix.append("friend")
        head = " ".join(prefix + ([type_text] if type_text else []) + [name])
        if kind == "variable":
            args = element.findtext("argsstring") or ""
            initializer = element_text(element.find("initializer")).strip()
            if initializer and not initializer.startswith(("{", "=")):
                initializer = f" = {initializer}"
            elif initializer.startswith("="):
                initializer = f" {initializer}"
            return f"{template}{head}{args}{tidy_type(initializer) if initializer else ''}"
        if kind in ("function", "signal", "slot", "friend"):
            params = [self._param_text(param) for param in element.findall("param")]
            qualifiers = self._qualifiers(element.findtext("argsstring") or "")
            if kind == "friend" and not element.findall("param") and "(" not in (
                    element.findtext("argsstring") or ""):
                return f"{template}{head}"
            one_line = f"{template}{head}({', '.join(params)}){qualifiers}"
            if len(one_line.splitlines()[-1]) <= 96 or not params:
                return one_line
            joined = ",\n    ".join(params)
            return f"{template}{head}(\n    {joined}){qualifiers}"
        return f"{template}{head}"

    def _template(self, element: ET.Element) -> str:
        """Return the `template <...>` line of a templated member, or an empty string."""
        params = element.find("templateparamlist")
        if params is None:
            return ""
        texts = []
        for param in params.findall("param"):
            text = tidy_type(element_text(param.find("type")))
            declname = param.findtext("declname")
            if declname:
                text += f" {declname}"
            default = element_text(param.find("defval")).strip()
            if default:
                text += f" = {default}"
            texts.append(text)
        return f"template <{', '.join(texts)}>\n"

    @staticmethod
    def _param_text(param: ET.Element) -> str:
        """Return one parameter of a function declaration."""
        text = tidy_type(element_text(param.find("type")))
        name = param.findtext("declname") or ""
        if name:
            text += f" {name}"
        text += param.findtext("array") or ""
        default = element_text(param.find("defval")).strip()
        if default:
            text += f" = {default}"
        return text.strip()

    @staticmethod
    def _qualifiers(argsstring: str) -> str:
        """Return what follows the parameter list in Doxygen's `argsstring`."""
        depth = 0
        for index, char in enumerate(argsstring):
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    tail = argsstring[index + 1:].strip()
                    tail = re.sub(r"\s*=\s*(0|default|delete)$", r" = \1", tail).strip()
                    return f" {tail}" if tail else ""
        return ""

    def _enumerators(self, element: ET.Element) -> None:
        """Write the table of an enumeration's values."""
        rows = []
        for value in element.findall("enumvalue"):
            name = value.findtext("name") or ""
            initializer = element_text(value.find("initializer")).strip().lstrip("=").strip()
            description = self.description(value.find("briefdescription"))
            detail = self.description(value.find("detaileddescription"))
            text = " ".join(filter(None, [description.inline(), detail.inline()]))
            rows.append(f"| {code_span(name)} | {code_span(initializer)} | {text} |")
        if rows:
            self.emit_block("| Enumerator | Value | Description |\n| --- | --- | --- |\n"
                            + "\n".join(rows))

    def _parameters(self, element: ET.Element, detail: Description) -> None:
        """Write the parameter table of a function, with the declared types."""
        params = element.findall("param")
        if element.get("kind") not in ("function", "signal", "slot", "friend", "define"):
            return
        rows = []
        for param in params:
            name = param.findtext("declname") or param.findtext("defname") or ""
            if not name:
                continue
            direction, text = detail.params.get(name, (None, ""))
            label = code_span(name) + (f" ({direction})" if direction else "")
            rows.append(f"| {label} | {self.linked_type(param.find('type'))} | {text} |")
        if rows:
            self.emit_block("**Parameters**\n\n| Name | Type | Description |\n"
                            "| --- | --- | --- |\n" + "\n".join(rows))

    def _tparams(self, detail: Description) -> None:
        """Write the template parameter table."""
        if detail.tparams:
            rows = [f"| {code_span(name)} | {text} |" for name, text in detail.tparams.items()]
            self.emit_block("**Template parameters**\n\n| Name | Description |\n"
                            "| --- | --- |\n" + "\n".join(rows))

    def _returns(self, element: ET.Element, detail: Description) -> None:
        """Write the return type and what the function returns."""
        if not detail.returns and not detail.retvals:
            return
        type_text = self.linked_type(element.find("type"))
        text = " ".join(detail.returns).strip()
        block = f"**Returns** {type_text}" + (f": {text}" if text else "")
        if detail.retvals:
            block += "\n\n| Value | Meaning |\n| --- | --- |\n" + "\n".join(
                f"| {code_span(value)} | {meaning} |" for value, meaning in detail.retvals)
        self.emit_block(block)

    def _exceptions(self, detail: Description) -> None:
        """Write the exceptions a function throws."""
        if detail.exceptions:
            rows = [f"| {f'[{code_span(name)}]({link})' if link else code_span(name)} | {text} |"
                    for name, link, text in detail.exceptions]
            self.emit_block("**Throws**\n\n| Exception | Condition |\n| --- | --- |\n"
                            + "\n".join(rows))

    def _labelled(self, detail: Description) -> None:
        """Write the preconditions, postconditions, "See also" and similar lists."""
        for title, items in detail.labelled.items():
            self.emit_block(f"**{title}**\n\n" + "\n".join(f"- {item}" for item in items))

    def _member_facts(self, member: Member) -> None:
        """Write where a member is declared and defined, and what it overrides."""
        element = member.element
        facts = []
        reimplements = element.find("reimplements")
        if reimplements is not None:
            link = self.model.target(reimplements.get("refid", ""), self.page)
            name = element_text(reimplements).strip()
            facts.append("Overrides " + (f"[{code_span(name)}]({link})" if link
                                         else code_span(name)))
        location = element.find("location")
        if location is not None:
            # A namespace member defined in a .cpp file reports the definition as its
            # location and the declaration separately.
            declared = location.get("declfile") or location.get("file", "")
            line = location.get("declline") or location.get("line")
            facts.append(f"Declared in [{code_span(f'{declared}:{line}')}]"
                         f"({self.source(declared, line)})")
            body = location.get("bodyfile")
            start = location.get("bodystart")
            if body and start and start != "-1" and (body, start) != (declared, line):
                facts.append(f"defined in [{code_span(f'{body}:{start}')}]"
                             f"({self.source(body, start)})")
        if facts:
            self.emit_block("<small>" + " · ".join(facts) + "</small>")

    def _inherited(self) -> None:
        """List the members inherited from other documented classes, grouped by class."""
        element = self.compound.element
        own = {m.refid for m in self.compound.members}
        groups: dict[str, list[str]] = {}
        listing = element.find("listofallmembers")
        if listing is None:
            return
        for entry in listing.findall("member"):
            refid = entry.get("refid", "")
            member = self.model.members.get(refid)
            if refid in own or member is None or member.page == self.page:
                continue
            if entry.get("prot") == "private":
                continue
            scope = entry.findtext("scope") or ""
            link = relative_link(self.page, member.page, member.anchor)
            suffix = "()" if member.kind in ("function", "signal", "slot") else ""
            groups.setdefault(scope, []).append(f"[{code_span(member.name + suffix)}]({link})")
        if groups:
            lines = ["## Inherited members {#inherited-members}", ""]
            for scope, links in groups.items():
                lines.append(f"**From {code_span(scope)}:** " + ", ".join(links))
                lines.append("")
            self.emit_block("\n".join(lines).rstrip())


@dataclasses.dataclass
class Reference:
    """The generated reference: pages and navigation.

    Attributes:
        pages: Markdown of every generated page, by path relative to the documentation root.
        nav: Navigation entries for the *C++ API* section, in MkDocs `nav` format.
        compound_pages: Page of every compound, by Doxygen id, for redirects.
        documented_files: Repository paths of the files that have a file comment.
        edit_sources: Repository file whose comments produce each page, by page path.
    """

    pages: dict[str, str]
    nav: list
    compound_pages: dict[str, str]
    documented_files: list[str]
    edit_sources: dict[str, str]


def build_reference(xml_dir: Path, root: Path, repo_url: str, git_ref: str) -> Reference:
    """Generate every page of the C++ reference and its navigation.

    Args:
        xml_dir: Doxygen's XML output directory.
        root: Repository root, for reading test files.
        repo_url: URL of the GitHub repository, without a trailing slash.
        git_ref: Commit or branch that source links point at.

    Returns:
        The pages, the navigation and the page of every compound.
    """
    model = Model(xml_dir)

    def source(path: str, line: str | None) -> str:
        """Return the GitHub URL of a repository file, at a line if one is given."""
        url = f"{repo_url}/blob/{git_ref}/{path}"
        return f"{url}#L{line}" if line else url

    pages = {}
    for compound in model.compounds.values():
        pages[compound.page] = PageWriter(model, compound, source, root).write()
    for part in PARTS:
        pages[part.page] = part_index(model, part)
    nav = navigation(model)
    compound_pages = {refid: c.page for refid, c in model.compounds.items()}
    documented = sorted(path for path, file in model.files.items()
                        if Writer(model, file.page).description(file.brief).inline())
    edit_sources = {c.page: c.file for c in model.compounds.values() if c.file}
    return Reference(dict(sorted(pages.items())), nav, compound_pages, documented, edit_sources)


def file_title(part: Part, path: str) -> str:
    """Return the title of a file in the navigation of its part."""
    if part.key == "library":
        return path.split("/include/nmeasim/", 1)[-1]
    if part.key == "internals":
        return re.sub(r"^src/(core|io)/src/", r"\1/", path)
    return path.removeprefix(part.file_prefix)


def part_index(model: Model, part: Part) -> str:
    """Return the index page of one part: its namespaces, types and files."""
    writer = Writer(model, part.page)
    lines = [f"# {part.title}", "", part.intro, ""]
    namespaces = [c for c in model.ordered(("namespace",)) if c.part == part.key]
    if namespaces:
        lines += ["## Namespaces", "", "| Namespace | Description |", "| --- | --- |"]
        for compound in namespaces:
            link = relative_link(part.page, compound.page)
            lines.append(f"| [{code_span(compound.name)}]({link}) | "
                         f"{writer.description(compound.brief).inline()} |")
        lines.append("")
    types = [c for c in model.ordered(("class", "struct", "union")) if c.part == part.key]
    if types:
        lines += ["## Types", "", "| Type | Description |", "| --- | --- |"]
        for compound in types:
            link = relative_link(part.page, compound.page)
            name = compound.name.replace("anonymous_namespace", "(anonymous namespace)")
            lines.append(f"| [{code_span(name)}]({link}) | "
                         f"{writer.description(compound.brief).inline()} |")
        lines.append("")
    files = [c for c in model.ordered(("file",)) if c.part == part.key]
    if files:
        lines += ["## Files", "", "| File | Description |", "| --- | --- |"]
        for compound in files:
            link = relative_link(part.page, compound.page)
            lines.append(f"| [{code_span(compound.name)}]({link}) | "
                         f"{writer.description(compound.brief).inline()} |")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def navigation(model: Model) -> list:
    """Return the navigation subtree of the reference in MkDocs `nav` format.

    Each part lists its namespaces, each with the types declared in it, then its files,
    each with the file-local types declared in it.
    """
    nav: list = [{"Overview": OVERVIEW_PAGE}]
    namespaces = {c.name for c in model.compounds.values() if c.kind == "namespace"}
    for part in PARTS:
        entries: list = [{"Overview": part.page}]
        for namespace in model.ordered(("namespace",)):
            if namespace.part != part.key:
                continue
            children = [c for c in model.ordered(("class", "struct", "union"))
                        if "anonymous_namespace{" not in c.name
                        and _namespace_of(c.name, namespaces) == namespace.name]
            if children:
                entries.append({namespace.name: [{"Overview": namespace.page}]
                                + [{c.title: c.page} for c in children]})
            else:
                entries.append({namespace.name: namespace.page})
        files = [c for c in model.ordered(("file",)) if c.part == part.key]
        if files:
            file_entries = []
            for file in files:
                local = [c for c in model.ordered(("class", "struct", "union"))
                         if c.file == file.name and "anonymous_namespace{" in c.name]
                title = file_title(part, file.name)
                if local:
                    file_entries.append({title: [{"Overview": file.page}]
                                         + [{c.title: c.page} for c in local]})
                else:
                    file_entries.append({title: file.page})
            label = {"library": "Headers", "internals": "Source files"}.get(part.key, "Files")
            entries.append({label: file_entries})
        nav.append({part.title: entries})
    return nav


def _namespace_of(name: str, namespaces: set[str]) -> str:
    """Return the innermost namespace of a qualified class name that has a page.

    Args:
        name: Qualified name of a class, struct or union.
        namespaces: Qualified names of the namespaces that have a page.
    """
    parts = name.split("::")
    for end in range(len(parts) - 1, 0, -1):
        candidate = "::".join(parts[:end])
        if candidate in namespaces:
            return candidate
    return ""


def main(argv: list[str]) -> int:
    """Write the generated pages of an XML directory to a directory, for inspection.

    Args:
        argv: Command-line arguments: the XML directory and the output directory.

    Returns:
        The process exit status.
    """
    if len(argv) != 3:
        print(__doc__.strip().splitlines()[0], file=sys.stderr)
        print("usage: cppreference.py XML_DIR OUTPUT_DIR", file=sys.stderr)
        return 2
    root = Path(__file__).resolve().parents[2]
    reference = build_reference(Path(argv[1]), root,
                                "https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X", "main")
    output = Path(argv[2])
    for path, text in reference.pages.items():
        target = output / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8")
    print(f"{len(reference.pages)} pages written to {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
