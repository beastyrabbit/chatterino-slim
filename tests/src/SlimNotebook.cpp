// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/hotkeys/HotkeyController.hpp"
#include "mocks/BaseApplication.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "Test.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/Notebook.hpp"

#include <QApplication>

using namespace chatterino;

namespace {

class MockApplication : public mock::BaseApplication
{
public:
    MockApplication()
        : windowManager(this->args_, this->paths_, this->settings, this->theme,
                        this->fonts)
    {
    }

    WindowManager *getWindows() override
    {
        return &this->windowManager;
    }
    HotkeyController *getHotkeys() override
    {
        return &this->hotkeys;
    }
    HotkeyController hotkeys;
    WindowManager windowManager;
};

class SlimNotebook : public Notebook
{
public:
    SlimNotebook()
        : Notebook(nullptr)
    {
    }

    using Notebook::setFillTabWidth;
    using Notebook::setHideSingleTab;
};

class SlimNotebookFixture : public ::testing::Test
{
protected:
    SlimNotebookFixture()
    {
        this->notebook.setAttribute(Qt::WA_DontShowOnScreen);
        this->notebook.setShowAddButton(false);
        this->notebook.resize(WIDTH, 300);
        this->notebook.show();
    }

    NotebookTab *add(const QString &title)
    {
        auto *tab = this->notebook.addPage(new QWidget, title);
        settle();
        return tab;
    }

    static void settle()
    {
        QApplication::processEvents();
    }

    static constexpr int WIDTH = 600;

    MockApplication app;
    SlimNotebook notebook;
};

}  // namespace

TEST_F(SlimNotebookFixture, SingleTabStaysVisibleByDefault)
{
    auto *tab = this->add("only");
    EXPECT_TRUE(tab->isVisible());
}

TEST_F(SlimNotebookFixture, HideSingleTab)
{
    this->notebook.setHideSingleTab(true);
    auto *first = this->add("first");
    EXPECT_FALSE(first->isVisible());

    auto *second = this->add("second");
    EXPECT_TRUE(first->isVisible());
    EXPECT_TRUE(second->isVisible());

    this->notebook.removePage(this->notebook.getPageAt(1));
    settle();
    EXPECT_FALSE(first->isVisible());
}

/// Without a tab row the page fills the whole notebook
TEST_F(SlimNotebookFixture, HiddenSingleTabGivesPageFullHeight)
{
    this->notebook.setHideSingleTab(true);
    this->add("only");
    auto *page = this->notebook.getPageAt(0);
    EXPECT_EQ(page->geometry(), this->notebook.rect());
}

TEST_F(SlimNotebookFixture, TabsKeepNaturalWidthByDefault)
{
    auto *first = this->add("a");
    auto *second = this->add("b");
    EXPECT_LT(second->geometry().right(), WIDTH / 2);
    EXPECT_LT(first->width(), WIDTH / 4);
}

TEST_F(SlimNotebookFixture, FillTabWidthSplitsWidthEqually)
{
    this->notebook.setFillTabWidth(true);
    std::vector<NotebookTab *> tabs{this->add("a"), this->add("bbbbbb"),
                                    this->add("ccc")};

    for (auto *tab : tabs)
    {
        EXPECT_NEAR(tab->width(), WIDTH / 3, 4)
            << tab->getTitle().toStdString();
    }
    EXPECT_LE(tabs.front()->geometry().left(), 4);
    EXPECT_NEAR(tabs.back()->geometry().right(), WIDTH - 1, 4);
}

TEST_F(SlimNotebookFixture, FillTabWidthWithOneVisibleTab)
{
    this->notebook.setFillTabWidth(true);
    auto *tab = this->add("only");
    EXPECT_NEAR(tab->width(), WIDTH, 4);
}
