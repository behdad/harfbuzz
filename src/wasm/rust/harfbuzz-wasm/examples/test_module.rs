//! A module that puts the toolkit through its paces.
//!
//! Harfbuzz's CI builds it with the checks of a debug build, puts it in
//! a font and shapes with that:
//!
//! ```sh
//! cargo build --target wasm32-unknown-unknown --features kurbo --example test_module
//! python3 src/addTable.py font.ttf test.ttf .../debug/examples/test_module.wasm
//! hb-shape --shapers=wasm --no-glyph-names --no-clusters --no-positions test.ttf a
//! ```
//!
//! The answer has one glyph for each check, 1 if it passed:
//! `[1|1|1|1|1|1|1|1|1]`. It is the same when hb-shape is given features.
use harfbuzz_wasm::{Features, Font, Glyph, GlyphBuffer};

#[no_mangle]
pub extern "C" fn shape(
    _shape_plan: u32,
    font_ref: u32,
    buf_ref: u32,
    features: u32,
    num_features: u32,
) -> i32 {
    let font = Font::from_ref(font_ref);
    let face = font.get_face();
    let mut checks: Vec<bool> = vec![];

    // The features. When there are none, Harfbuzz hands over no array.
    let features = Features::from_raw(features, num_features);
    let count = features.iter().count();
    checks.push(features.len() == num_features as usize && features.len() == count);

    // A glyph that the font does not have has no outline.
    checks.push(font.get_outline(60000).is_empty());

    // The first glyph of the text has one.
    let text = GlyphBuffer::from_ref(buf_ref);
    let glyph = font.get_glyph(text.glyphs.first().map_or(0, |g| g.codepoint), 0);
    checks.push(!text.glyphs.is_empty() && glyph != 0);
    let outline = font.get_outline(glyph);
    checks.push(outline.iter().any(|path| !path.elements().is_empty()));
    drop(text);

    // A table that every font has, and one that none has.
    let table = face.reference_table("maxp");
    checks.push(!table.is_empty() && table.len() == table.as_slice().len());
    let missing = face.reference_table("ZZZZ");
    checks.push(missing.is_empty() && [missing.len(), missing.as_slice().len()] == [0, 0]);

    checks.push(face.reference_table("maxp").as_slice() == table.as_slice());

    // The module itself is a table of the font it is in. An instance has
    // room for the font and 2MiB more of what Harfbuzz copies for it, and
    // none for as many copies of the module as this: each is to be
    // freed when its `Blob` is dropped.
    let size = face.reference_table("Wasm").len();
    let copies = (16 << 20) / size.max(1) + 2;
    checks.push(size > 0 && (0..copies).all(|_| face.reference_table("Wasm").len() == size));

    // The answer goes back to Harfbuzz when the buffer is dropped.
    let mut answer = GlyphBuffer::from_ref(buf_ref);
    checks.push(!answer.glyphs.is_empty());
    answer.glyphs.clear();
    for passed in checks {
        answer.glyphs.push(Glyph {
            codepoint: u32::from(passed),
            cluster: 0,
            x_advance: 0,
            y_advance: 0,
            x_offset: 0,
            y_offset: 0,
            flags: 0,
        });
    }
    1
}
