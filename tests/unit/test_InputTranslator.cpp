// Unit tests for InputTranslator.

#include <gtest/gtest.h>

#include "adapters/InputTranslator.hpp"

using editor::adapters::translate;
using editor::core::Key;

// -- Vim motion keys ----------------------------------------------------------

TEST(InputTranslatorTest, TranslatesH) {
    EXPECT_EQ(translate(ftxui::Event::Character('h')), Key::H);
}

TEST(InputTranslatorTest, TranslatesJ) {
    EXPECT_EQ(translate(ftxui::Event::Character('j')), Key::J);
}

TEST(InputTranslatorTest, TranslatesK) {
    EXPECT_EQ(translate(ftxui::Event::Character('k')), Key::K);
}

TEST(InputTranslatorTest, TranslatesL) {
    EXPECT_EQ(translate(ftxui::Event::Character('l')), Key::L);
}

TEST(InputTranslatorTest, TranslatesGLower) {
    EXPECT_EQ(translate(ftxui::Event::Character('g')), Key::G_lower);
}

TEST(InputTranslatorTest, TranslatesGUpper) {
    EXPECT_EQ(translate(ftxui::Event::Character('G')), Key::G_upper);
}

TEST(InputTranslatorTest, TranslatesDollar) {
    EXPECT_EQ(translate(ftxui::Event::Character('$')), Key::Dollar);
}

TEST(InputTranslatorTest, TranslatesZero) {
    EXPECT_EQ(translate(ftxui::Event::Character('0')), Key::Zero);
}

TEST(InputTranslatorTest, TranslatesI) {
    EXPECT_EQ(translate(ftxui::Event::Character('i')), Key::I);
}

TEST(InputTranslatorTest, TranslatesA) {
    EXPECT_EQ(translate(ftxui::Event::Character('a')), Key::A);
}

// -- Control keys -------------------------------------------------------------

TEST(InputTranslatorTest, TranslatesEscape) {
    EXPECT_EQ(translate(ftxui::Event::Escape), Key::Escape);
}

TEST(InputTranslatorTest, TranslatesBackspace) {
    EXPECT_EQ(translate(ftxui::Event::Backspace), Key::Backspace);
}

TEST(InputTranslatorTest, TranslatesReturn) {
    EXPECT_EQ(translate(ftxui::Event::Return), Key::Return);
}

// -- Unmapped keys return nullopt ---------------------------------------------

TEST(InputTranslatorTest, UnmappedCharacterReturnsNullopt) {
    EXPECT_EQ(translate(ftxui::Event::Character('z')), std::nullopt);
}

TEST(InputTranslatorTest, ArrowKeyReturnsNullopt) {
    EXPECT_EQ(translate(ftxui::Event::ArrowUp), std::nullopt);
}
