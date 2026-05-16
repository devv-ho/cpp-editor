#include <gtest/gtest.h>

#include "adapters/InputTranslator.hpp"

using editor::adapters::translate;
using editor::core::Command;

TEST(InputTranslatorTest, TranslatesH) {
    EXPECT_EQ(translate(ftxui::Event::Character('h')), Command::move_left);
}

TEST(InputTranslatorTest, TranslatesJ) {
    EXPECT_EQ(translate(ftxui::Event::Character('j')), Command::move_down);
}

TEST(InputTranslatorTest, TranslatesK) {
    EXPECT_EQ(translate(ftxui::Event::Character('k')), Command::move_up);
}

TEST(InputTranslatorTest, TranslatesL) {
    EXPECT_EQ(translate(ftxui::Event::Character('l')), Command::move_right);
}

TEST(InputTranslatorTest, TranslatesGLower) {
    EXPECT_EQ(translate(ftxui::Event::Character('g')), Command::pending_g);
}

TEST(InputTranslatorTest, TranslatesGUpper) {
    EXPECT_EQ(translate(ftxui::Event::Character('G')), Command::move_bottom);
}

TEST(InputTranslatorTest, TranslatesDollar) {
    EXPECT_EQ(translate(ftxui::Event::Character('$')), Command::move_eol);
}

TEST(InputTranslatorTest, TranslatesZero) {
    EXPECT_EQ(translate(ftxui::Event::Character('0')), Command::move_sol);
}

TEST(InputTranslatorTest, TranslatesI) {
    EXPECT_EQ(translate(ftxui::Event::Character('i')), Command::enter_insert);
}

TEST(InputTranslatorTest, TranslatesA) {
    EXPECT_EQ(translate(ftxui::Event::Character('a')), Command::enter_insert_after);
}

TEST(InputTranslatorTest, TranslatesEscape) {
    EXPECT_EQ(translate(ftxui::Event::Escape), Command::enter_normal);
}

TEST(InputTranslatorTest, TranslatesBackspace) {
    EXPECT_EQ(translate(ftxui::Event::Backspace), Command::backspace);
}

TEST(InputTranslatorTest, TranslatesReturn) {
    EXPECT_EQ(translate(ftxui::Event::Return), Command::insert_newline);
}

TEST(InputTranslatorTest, UnmappedCharacterReturnsNullopt) {
    EXPECT_EQ(translate(ftxui::Event::Character('z')), std::nullopt);
}

TEST(InputTranslatorTest, ArrowKeyReturnsNullopt) {
    EXPECT_EQ(translate(ftxui::Event::ArrowUp), std::nullopt);
}
