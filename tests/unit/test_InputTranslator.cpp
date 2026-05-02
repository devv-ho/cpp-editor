// Unit tests for InputTranslator.

#include <gtest/gtest.h>

#include "adapters/InputTranslator.hpp"

using editor::adapters::translate;
using editor::core::Key;

// -- Vim motion keys ----------------------------------------------------------

TEST(InputTranslatorTest, TranslatesH) {
    EXPECT_EQ(translate(ftxui::Event::Character('h')), Key::h);
}

TEST(InputTranslatorTest, TranslatesJ) {
    EXPECT_EQ(translate(ftxui::Event::Character('j')), Key::j);
}

TEST(InputTranslatorTest, TranslatesK) {
    EXPECT_EQ(translate(ftxui::Event::Character('k')), Key::k);
}

TEST(InputTranslatorTest, TranslatesL) {
    EXPECT_EQ(translate(ftxui::Event::Character('l')), Key::l);
}

TEST(InputTranslatorTest, TranslatesGLower) {
    EXPECT_EQ(translate(ftxui::Event::Character('g')), Key::g);
}

TEST(InputTranslatorTest, TranslatesGUpper) {
    EXPECT_EQ(translate(ftxui::Event::Character('G')), Key::G);
}

TEST(InputTranslatorTest, TranslatesDollar) {
    EXPECT_EQ(translate(ftxui::Event::Character('$')), Key::dollar);
}

TEST(InputTranslatorTest, TranslatesZero) {
    EXPECT_EQ(translate(ftxui::Event::Character('0')), Key::zero);
}

TEST(InputTranslatorTest, TranslatesI) {
    EXPECT_EQ(translate(ftxui::Event::Character('i')), Key::i);
}

TEST(InputTranslatorTest, TranslatesA) {
    EXPECT_EQ(translate(ftxui::Event::Character('a')), Key::a);
}

// -- Control keys -------------------------------------------------------------

TEST(InputTranslatorTest, TranslatesEscape) {
    EXPECT_EQ(translate(ftxui::Event::Escape), Key::escape);
}

TEST(InputTranslatorTest, TranslatesBackspace) {
    EXPECT_EQ(translate(ftxui::Event::Backspace), Key::backspace);
}

TEST(InputTranslatorTest, TranslatesReturn) {
    EXPECT_EQ(translate(ftxui::Event::Return), Key::enter);
}

// -- Unmapped keys return nullopt ---------------------------------------------

TEST(InputTranslatorTest, UnmappedCharacterReturnsNullopt) {
    EXPECT_EQ(translate(ftxui::Event::Character('z')), std::nullopt);
}

TEST(InputTranslatorTest, ArrowKeyReturnsNullopt) {
    EXPECT_EQ(translate(ftxui::Event::ArrowUp), std::nullopt);
}
