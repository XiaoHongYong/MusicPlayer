#!/usr/bin/env python3
import unittest

from i18n_extract import (
    extract_cpp_macros,
    extract_menu_json,
    extract_skin_xml,
    extract_web_t_calls,
    parse_concatenated_c_strings,
)


class CppExtractTest(unittest.TestCase):
    def test_macros_and_skip_non_literals(self):
        src = r'''
        a = _TL("Play");
        b = _TLT("Failed to get searching lyrics results.");
        c = _TLM("Toggle always on top");
        d = _TL(label.c_str());
        e = _TLT("foo "
                 "bar");
        f = not_TL("no");
        g = _TLONG("no");
        '''
        keys = [x.key for x in extract_cpp_macros(src, "x.cpp")]
        self.assertEqual(
            keys,
            [
                "Play",
                "Failed to get searching lyrics results.",
                "Toggle always on top",
                "foo bar",
            ],
        )

    def test_skips_comments(self):
        src = '''
        // _TL("nope")
        /* _TLT("nope2") */
        x = _TL("yes");
        '''
        keys = [x.key for x in extract_cpp_macros(src, "x.cpp")]
        self.assertEqual(keys, ["yes"])

    def test_unescape(self):
        src = r'_TL("say \"hi\"");'
        keys = [x.key for x in extract_cpp_macros(src, "x.cpp")]
        self.assertEqual(keys, ['say "hi"'])

    def test_concat_parser(self):
        text = '"a"  "b"'
        got = parse_concatenated_c_strings(text, 0)
        assert got is not None
        self.assertEqual(got[0], "ab")


class WebExtractTest(unittest.TestCase):
    def test_t_calls(self):
        src = '''
        t("Home")
        t('Songs')
        t(`Playlists`)
        t("Hello {name}", { name })
        nott("no")
        function t(key: string) {}
        '''
        keys = [x.key for x in extract_web_t_calls(src, "x.tsx")]
        self.assertEqual(keys, ["Home", "Songs", "Playlists", "Hello {name}"])


class MenuAndSkinExtractTest(unittest.TestCase):
    def test_menu_json(self):
        import json
        import tempfile
        from pathlib import Path

        data = {
            "MainWndMenu": [
                ["&File", [["&Open File...", "ID_PL_OPEN_FILE"], ["separator"]]],
                ["INSERT_SKIN_POS"],
                ["4", "ID_RATE_LYR_4"],
            ]
        }
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "menu.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            keys = [x.key for x in extract_menu_json(path)]
        self.assertEqual(keys, ["&File", "&Open File..."])

    def test_skin_xml(self):
        import tempfile
        from pathlib import Path

        xml = '''
        <Text Text="Playlist"/>
        <NormalEdit PlaceHolder="Search music"/>
        <button ToolTip="Lyrics"/>
        <NormalTextBt Text="&amp;Save"/>
        <Text Text=""/>
        <Text Text="TRUE"/>
        '''
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "main.xml").write_text(xml, encoding="utf-8")
            keys = [x.key for x in extract_skin_xml(root)]
        self.assertEqual(keys, ["Playlist", "Search music", "Lyrics", "&Save"])


if __name__ == "__main__":
    unittest.main()
