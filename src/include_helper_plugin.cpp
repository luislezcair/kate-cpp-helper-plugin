/**
 * \file
 *
 * \brief Class \c IncludeHelperPlugin (implementation)
 *
 * \date Sun Jan 29 09:15:53 MSK 2012 -- Initial design
 */
/*
 * KateIncludeHelperPlugin is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * KateIncludeHelperPlugin is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Project specific includes
#include <src/include_helper_plugin.h>

// Standard includes
#include <KAboutData>
#include <KDebug>
#include <KDirSelectDialog>
#include <KPluginFactory>
#include <KPluginLoader>

namespace {
inline int area()
{
    static int s_area = KDebug::registerArea("include-helper", true);
    return s_area;
}
}                                                           // anonymous namespace


K_PLUGIN_FACTORY(IncludeHelperPluginFactory, registerPlugin<IncludeHelperPlugin>();)
K_EXPORT_PLUGIN(
    IncludeHelperPluginFactory(
        KAboutData(
            "kateincludehelperplugin"
          , "kate_includehelper_plugin"
          , ki18n("Include Helper Plugin")
          , "0.1"
          , ki18n("Helps to work w/ C/C++ headers little more easy")
          )
      )
  )

//BEGIN IncludeHelperPlugin
IncludeHelperPlugin::IncludeHelperPlugin(
    QObject* application
  , const QList<QVariant>&
  )
  : Kate::Plugin(static_cast<Kate::Application*>(application), "kate_includehelper_plugin")
{
}

Kate::PluginView* IncludeHelperPlugin::createView(Kate::MainWindow* parent)
{
    return new IncludeHelperPluginView(parent, IncludeHelperPluginFactory::componentData());
}

Kate::PluginConfigPage* IncludeHelperPlugin::configPage(uint number, QWidget* parent, const char*)
{
    assert("This plugin have the only configuration page" && number == 0);
    if (number != 0)
        return nullptr;
    IncludeHelperPluginGlobalConfigPage* cfg_page = new IncludeHelperPluginGlobalConfigPage(parent, this);
    // Subscribe self for config updates
    connect(
        cfg_page
        , SIGNAL(sessionDirsUpdated(const QStringList&))
        , this
        , SLOT(updateSessionDirs(const QStringList&))
        );
    return cfg_page;
}

void IncludeHelperPlugin::readSessionConfig(KConfigBase* config, const QString& groupPrefix)
{
    // Read session config
    KConfigGroup scg(config, groupPrefix + ":include-helper");
    QStringList session_dirs = scg.readPathEntry("ConfiguredDirs", QStringList());
    kDebug() << "Got per session configured include path list: " << session_dirs;
    // Read global config
    KConfigGroup gcg(KGlobal::config(), "IncludeHelper");
    QStringList dirs = gcg.readPathEntry("ConfiguredDirs", QStringList());
    kDebug() << "Got global configured include path list: " << dirs;
    // Assign configuration
    m_session_dirs = session_dirs;
    m_global_dirs = dirs;
}

void IncludeHelperPlugin::writeSessionConfig(KConfigBase* config, const QString& groupPrefix)
{
    // Write session config
    kDebug() << "Write per session configured include path list: " << m_session_dirs;
    KConfigGroup scg(config, groupPrefix + ":include-helper");
    scg.writePathEntry("ConfiguredDirs", m_session_dirs);
    scg.sync();
    // Read global config
    kDebug() << "Write global configured include path list: " << m_global_dirs;
    KConfigGroup gcg(KGlobal::config(), "IncludeHelper");
    gcg.writePathEntry("ConfiguredDirs", m_global_dirs);
    gcg.sync();
}

void IncludeHelperPlugin::updateSessionDirs(const QStringList& list)
{
    m_session_dirs = list;
    kDebug() << "Session dirs updated to: " << m_session_dirs;
}

void IncludeHelperPlugin::updateGlobalDirs(const QStringList& list)
{
    m_global_dirs = list;
    kDebug() << "Global dirs updated to: " << m_global_dirs;
}
//END IncludeHelperPlugin

//BEGIN IncludeHelperPluginGlobalConfigPage
IncludeHelperPluginGlobalConfigPage::IncludeHelperPluginGlobalConfigPage(
    QWidget* parent
  , IncludeHelperPlugin* plugin
  )
  : Kate::PluginConfigPage(parent)
  , m_plugin(plugin)
{
    m_configuration_ui.setupUi(this);
    m_configuration_ui.addToGlobalButton->setIcon(KIcon("list-add"));
    m_configuration_ui.delFromGlobalButton->setIcon(KIcon("list-remove"));
    m_configuration_ui.addToSessionButton->setIcon(KIcon("list-add"));
    m_configuration_ui.delFromSessionButton->setIcon(KIcon("list-remove"));

    // Connect add/del buttons to actions
    connect(m_configuration_ui.addToGlobalButton, SIGNAL(clicked()), this, SLOT(addGlobalIncludeDir()));
    connect(m_configuration_ui.delFromGlobalButton, SIGNAL(clicked()), this, SLOT(delGlobalIncludeDir()));
    connect(m_configuration_ui.addToSessionButton, SIGNAL(clicked()), this, SLOT(addSessionIncludeDir()));
    connect(m_configuration_ui.delFromSessionButton, SIGNAL(clicked()), this, SLOT(delSessionIncludeDir()));

    // Populate configuration w/ dirs
    reset();
}

void IncludeHelperPluginGlobalConfigPage::apply()
{
    // Notify about configuration changes
    {
        QStringList dirs;
        for (int i = 0; i < m_configuration_ui.sessionDirsList->count(); ++i)
        {
            dirs.append(m_configuration_ui.sessionDirsList->item(i)->text());
        }
        Q_EMIT(sessionDirsUpdated(dirs));
    }
    {
        QStringList dirs;
        for (int i = 0; i < m_configuration_ui.globalDirsList->count(); ++i)
        {
            dirs.append(m_configuration_ui.globalDirsList->item(i)->text());
        }
        Q_EMIT(globalDirsUpdated(dirs));
    }
}

void IncludeHelperPluginGlobalConfigPage::reset()
{
    kDebug() << "Reseting configuration";
    // Put dirs to the list
    m_configuration_ui.globalDirsList->addItems(m_plugin->globalDirs());
    m_configuration_ui.sessionDirsList->addItems(m_plugin->sessionDirs());
}

void IncludeHelperPluginGlobalConfigPage::addSessionIncludeDir()
{
    KUrl dir_uri = KDirSelectDialog::selectDirectory(KUrl(), true, this);
    if (dir_uri != KUrl())
    {
        const QString& dir_str = dir_uri.toLocalFile();
        if (!contains(dir_str, m_configuration_ui.sessionDirsList))
            new QListWidgetItem(dir_str, m_configuration_ui.sessionDirsList);
    }
}

void IncludeHelperPluginGlobalConfigPage::delSessionIncludeDir()
{
    delete m_configuration_ui.sessionDirsList->currentItem();
}

void IncludeHelperPluginGlobalConfigPage::addGlobalIncludeDir()
{
    KUrl dir_uri = KDirSelectDialog::selectDirectory(KUrl(), true, this);
    if (dir_uri != KUrl())
    {
        const QString& dir_str = dir_uri.toLocalFile();
        if (!contains(dir_str, m_configuration_ui.globalDirsList))
            new QListWidgetItem(dir_str, m_configuration_ui.globalDirsList);
    }
}

void IncludeHelperPluginGlobalConfigPage::delGlobalIncludeDir()
{
    delete m_configuration_ui.globalDirsList->currentItem();
}

bool IncludeHelperPluginGlobalConfigPage::contains(const QString& dir, const KListWidget* list)
{
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->text() == dir)
            return true;
    return false;
}
//END IncludeHelperPluginGlobalConfigPage

//BEGIN IncludeHelperPluginView

IncludeHelperPluginView::IncludeHelperPluginView(Kate::MainWindow* mw, const KComponentData& data)
  : Kate::PluginView(mw)
  , Kate::XMLGUIClient(data)
{

}
//END IncludeHelperPluginView
