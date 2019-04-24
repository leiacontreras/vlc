/*****************************************************************************
 * complete_preferences.cpp : "Normal preferences"
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QApplication>
#include <QLabel>
#include <QTreeWidget>
#include <QVariant>
#include <QString>
#include <QFont>
#include <QGroupBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QGridLayout>

#include "dialogs/preferences/complete_preferences.hpp"
#include "dialogs/preferences/preferences_widgets.hpp"

#include <vlc_intf_strings.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>
#include <assert.h>

#define ITEM_HEIGHT 25

/*********************************************************************
 * The Tree
 *********************************************************************/
PrefsTree::PrefsTree( qt_intf_t *_p_intf, QWidget *_parent,
                      module_t **p_list, size_t count ) :
                            QTreeWidget( _parent ), p_intf( _p_intf )
{
    b_show_only_loaded = false;
    /* General Qt options */
    setAlternatingRowColors( true );
    setHeaderHidden( true );

    setIconSize( QSize( ITEM_HEIGHT, ITEM_HEIGHT ) );
    setTextElideMode( Qt::ElideNone );

    setUniformRowHeights( true );
    CONNECT( this, itemExpanded(QTreeWidgetItem*), this, resizeColumns() );

    int last_cat = CAT_UNKNOWN;
    int cat = CAT_UNKNOWN;
    int subcat = SUBCAT_UNKNOWN;
    QTreeWidgetItem *cat_item = nullptr;

    /* Create base cat/subcat tree from core config set */
    unsigned confsize;
    module_t *p_module = module_get_main();
    module_config_t *const p_config = module_config_get (p_module, &confsize);
    for( size_t i = 0; i < confsize; i++ )
    {
        module_config_t *p_item = p_config + i;

        if( p_item->i_type != CONFIG_SUBCATEGORY )
            continue;

        subcat = p_item->value.i;
        cat = vlc_config_cat_FromSubcat(subcat);

        if( cat == CAT_UNKNOWN || cat == CAT_HIDDEN )
            continue;

        /* Create parent cat node? */
        if( cat != last_cat )
        {
            cat_item = createCatNode( cat );
            last_cat = cat;
            /* Merge general subcat properties */
            setCatGeneralSubcat( cat_item, vlc_config_cat_GetGeneralSubcat( cat ) );
        }

        if( !vlc_config_subcat_IsGeneral(subcat) )
            createSubcatNode( cat_item, subcat );
    }
    module_config_free( p_config );

    /* Add nodes for plugins */
    /* A plugin gets one node for its entire set, located based on the first used subcat */
    for( size_t i = 0; i < count; i++ )
    {
        p_module = p_list[i];

        /* Main module is already covered above */
        if( module_is_main( p_module) )
            continue;

        unsigned confsize;
        module_config_t *const p_config = module_config_get (p_module, &confsize);

        /* Get the first (used) subcat */
        cat = CAT_UNKNOWN;
        subcat = SUBCAT_UNKNOWN;
        bool has_options = false;
        for (size_t i = 0; i < confsize; i++)
        {
            const module_config_t *p_item = p_config + i;

            if( p_item->i_type == CONFIG_SUBCATEGORY )
            {
                subcat = p_item->value.i;
                cat = vlc_config_cat_FromSubcat( subcat );
                continue;
            }

            if( CONFIG_ITEM(p_item->i_type) )
            {
                has_options = true;
                /* If cat lookup worked, we can stop now, else keep looking */
                if( cat != CAT_UNKNOWN )
                    break;
            }
        }
        module_config_free (p_config);

        /* No options, or definitely no place to place it in the tree - ignore */
        if( !has_options || cat == CAT_UNKNOWN || cat == CAT_HIDDEN )
            continue;

        /* Locate the category item */
        /* If not found (very unlikely), we will create it */
        cat_item = findCatItem( cat );
        if ( !cat_item )
        {
            cat_item = createCatNode( cat );
            // Merge general subcat properties
            setCatGeneralSubcat( cat_item, vlc_config_cat_GetGeneralSubcat( cat ) );
        }

        /* Locate the subcategory item */
        /* If not found (was not used in the core option set - quite possible), we will create it */
        QTreeWidgetItem *subcat_item = findSubcatItem( subcat );
        if( !subcat_item )
            subcat_item = createSubcatNode( cat_item, subcat );

        createPluginNode( subcat_item, p_module );
    }

    /* We got everything, just sort a bit */
    sortItems( 0, Qt::AscendingOrder );

    resizeColumnToContents( 0 );
}

QTreeWidgetItem *PrefsTree::createCatNode( int cat )
{
    QTreeWidgetItem *item = new QTreeWidgetItem();

    PrefsItemData *data = new PrefsItemData( this );
    data->i_type = PrefsItemData::TYPE_CATEGORY;
    data->cat_id = cat;
    data->subcat_id = SUBCAT_UNKNOWN;
    data->name = qfu( vlc_config_cat_GetName( cat ) );
    data->help = qfu( vlc_config_cat_GetHelp( cat ) );

    QIcon icon;
    switch( cat )
    {
#define CI(a,b) case a: icon = QIcon( b );break
        CI( CAT_AUDIO,     ":/prefsmenu/advanced/audio.svg"    );
        CI( CAT_VIDEO,     ":/prefsmenu/advanced/video.svg"    );
        CI( CAT_INPUT,     ":/prefsmenu/advanced/codec.svg"    );
        CI( CAT_SOUT,      ":/prefsmenu/advanced/sout.svg"     );
        CI( CAT_ADVANCED,  ":/prefsmenu/advanced/extended.svg" );
        CI( CAT_PLAYLIST,  ":/prefsmenu/advanced/playlist.svg" );
        CI( CAT_INTERFACE, ":/prefsmenu/advanced/intf.svg"     );
#undef CI
        default: break;
    }

    item->setText( 0, data->name );
    item->setIcon( 0, icon );
    item->setData( 0, Qt::UserRole, QVariant::fromValue( data ) );
    //current_item->setSizeHint( 0, QSize( -1, ITEM_HEIGHT ) );

    int cat_index = vlc_config_cat_IndexOf( cat );
    this->catMap[cat_index] = item;

    addTopLevelItem( item );
    expandItem( item );

    return item;
}

QTreeWidgetItem *PrefsTree::createSubcatNode( QTreeWidgetItem * cat, int subcat )
{
    assert( cat );

    QTreeWidgetItem *item = new QTreeWidgetItem();

    PrefsItemData *data = new PrefsItemData( this );
    data->i_type = PrefsItemData::TYPE_SUBCATEGORY;
    data->cat_id = CAT_UNKNOWN;
    data->subcat_id = subcat;
    data->name = qfu( vlc_config_subcat_GetName( subcat ) );
    data->help = qfu( vlc_config_subcat_GetHelp( subcat ) );

    item->setText( 0, data->name );
    item->setData( 0, Qt::UserRole, QVariant::fromValue( data ) );
    //item->setSizeHint( 0, QSize( -1, ITEM_HEIGHT ) );

    int subcat_index = vlc_config_subcat_IndexOf( subcat );
    this->subcatMap[subcat_index] = item;

    cat->addChild( item );

    return item;
}

void PrefsTree::createPluginNode( QTreeWidgetItem * parent, module_t *mod )
{
    assert( parent );

    QTreeWidgetItem *item = new QTreeWidgetItem();

    PrefsItemData *data = new PrefsItemData( this );
    data->i_type = PrefsItemData::TYPE_MODULE;
    data->cat_id = CAT_UNKNOWN;
    data->subcat_id = SUBCAT_UNKNOWN;
    data->p_module = mod;
    data->psz_shortcut = strdup( module_get_object( mod ) );
    data->name = qfut( module_get_name( mod, false ) );
    const char *help = module_get_help( mod );
    if( help )
        data->help = qfut( help );
    else
        data->help.clear();

    item->setText( 0, data->name );
    item->setData( 0, Qt::UserRole, QVariant::fromValue( data ) );
    //item->setSizeHint( 0, QSize( -1, ITEM_HEIGHT ) );

    parent->addChild( item );
}

void PrefsTree::setCatGeneralSubcat( QTreeWidgetItem *cat, int subcat )
{
    assert( cat );
    // Get data object of the cat
    PrefsItemData *data = cat->data( 0, Qt::UserRole ).value<PrefsItemData *>();
    // Adjust it
    data->i_type = PrefsItemData::TYPE_CATSUBCAT;
    data->subcat_id = subcat;
    data->name = qfu( vlc_config_subcat_GetName( subcat ) );
    data->help = qfu( vlc_config_subcat_GetHelp( subcat ) );

    int subcat_index = vlc_config_subcat_IndexOf( subcat );
    this->subcatMap[subcat_index] = cat;
}

QTreeWidgetItem *PrefsTree::findCatItem( int cat )
{
    int cat_index = vlc_config_cat_IndexOf( cat );
    return this->catMap[cat_index];
}

QTreeWidgetItem *PrefsTree::findSubcatItem( int subcat )
{
    int subcat_index = vlc_config_subcat_IndexOf( subcat );
    return this->subcatMap[subcat_index];
}

void PrefsTree::applyAll()
{
    doAll( false );
}

void PrefsTree::cleanAll()
{
    doAll( true );
}

void PrefsTree::doAll( bool doclean )
{
    for( int i_cat_index = 0 ; i_cat_index < topLevelItemCount();
             i_cat_index++ )
    {
        QTreeWidgetItem *cat_item = topLevelItem( i_cat_index );
        for( int i_sc_index = 0; i_sc_index < cat_item->childCount();
                 i_sc_index++ )
        {
            QTreeWidgetItem *sc_item = cat_item->child( i_sc_index );
            for( int i_module = 0 ; i_module < sc_item->childCount();
                     i_module++ )
            {
                PrefsItemData *data = sc_item->child( i_module )->
                               data( 0, Qt::UserRole).value<PrefsItemData *>();
                if( data->panel && doclean )
                {
                    delete data->panel;
                    data->panel = NULL;
                }
                else if( data->panel )
                    data->panel->apply();
            }
            PrefsItemData *data = sc_item->data( 0, Qt::UserRole).
                                            value<PrefsItemData *>();
            if( data->panel && doclean )
            {
                delete data->panel;
                data->panel = NULL;
            }
            else if( data->panel )
                data->panel->apply();
        }
        PrefsItemData *data = cat_item->data( 0, Qt::UserRole).
                                            value<PrefsItemData *>();
        if( data->panel && doclean )
        {
            delete data->panel;
            data->panel = NULL;
        }
        else if( data->panel )
            data->panel->apply();
    }
}

/* apply filter on tree item and recursively on its sub items
 * returns whether the item was filtered */
bool PrefsTree::filterItems( QTreeWidgetItem *item, const QString &text,
                           Qt::CaseSensitivity cs )
{
    bool sub_filtered = true;

    for( int i = 0; i < item->childCount(); i++ )
    {
        QTreeWidgetItem *sub_item = item->child( i );
        if ( !filterItems( sub_item, text, cs ) )
        {
            /* not all the sub items were filtered */
            sub_filtered = false;
        }
    }

    PrefsItemData *data = item->data( 0, Qt::UserRole ).value<PrefsItemData *>();

    bool filtered = sub_filtered && !data->contains( text, cs );
    if ( b_show_only_loaded && sub_filtered && !data->b_loaded )
        filtered = true;
    item->setExpanded( !sub_filtered );
    item->setHidden( filtered );

    return filtered;
}


/* collapse item if it's not selected or one of its sub items
 * returns whether the item was collapsed */
bool PrefsTree::collapseUnselectedItems( QTreeWidgetItem *item )
{
    bool sub_collapsed = true;

    for( int i = 0; i < item->childCount(); i++ )
    {
        QTreeWidgetItem *sub_item = item->child( i );
        if ( !collapseUnselectedItems( sub_item ) )
        {
            /* not all the sub items were collapsed */
            sub_collapsed = false;
        }
    }

    bool collapsed = sub_collapsed && !item->isSelected();
    item->setExpanded( !sub_collapsed );
    item->setHidden( false );

    return collapsed;
}

static void populateLoadedSet( QSet<QString> *loaded, vlc_object_t *p_node )
{
    Q_ASSERT( loaded );
    char *name = vlc_object_get_name( p_node );
    if ( !EMPTY_STR( name ) ) loaded->insert( QString( name ) );
    free( name );

    size_t count = 0, size;
    vlc_object_t **tab = NULL;

    do
    {
        delete[] tab;
        size = count;
        tab = new vlc_object_t *[size];
        count = vlc_list_children(p_node, tab, size);
    }
    while (size < count);

    for (size_t i = 0; i < count ; i++)
    {
        populateLoadedSet( loaded, tab[i] );
        vlc_object_release(tab[i]);
    }

    delete[] tab;
}

/* Updates the PrefsItemData loaded status to reflect currently
 * running modules */
void PrefsTree::updateLoadedStatus( QTreeWidgetItem *item = NULL,
                                    QSet<QString> *loaded = NULL )
{
    bool b_release = false;

    if( loaded == NULL )
    {
        vlc_object_t *p_root = VLC_OBJECT( vlc_object_instance(p_intf) );
        loaded = new QSet<QString>();
        populateLoadedSet( loaded, p_root );
        b_release = true;
    }

    if ( item == NULL )
    {
        for( int i = 0 ; i < topLevelItemCount(); i++ )
            updateLoadedStatus( topLevelItem( i ), loaded );
    }
    else
    {
        PrefsItemData *data = item->data( 0, Qt::UserRole )
                .value<PrefsItemData *>();
        data->b_loaded = loaded->contains( QString( data->psz_shortcut ) );

        for( int i = 0; i < item->childCount(); i++ )
            updateLoadedStatus( item->child( i ), loaded );
    }

    if ( b_release )
        delete loaded;
}

/* apply filter on tree */
void PrefsTree::filter( const QString &text )
{
    bool clear_filter = text.isEmpty() && ! b_show_only_loaded;

    updateLoadedStatus();

    for( int i = 0 ; i < topLevelItemCount(); i++ )
    {
        QTreeWidgetItem *cat_item = topLevelItem( i );
        if ( clear_filter )
        {
            collapseUnselectedItems( cat_item );
        }
        else
        {
            filterItems( cat_item, text, Qt::CaseInsensitive );
        }
    }
}

void PrefsTree::setLoadedOnly( bool b_only )
{
    b_show_only_loaded = b_only;
    filter( "" );
}

void PrefsTree::resizeColumns()
{
    resizeColumnToContents( 0 );
}

PrefsItemData::PrefsItemData( QObject *_parent ) : QObject( _parent )
{
    panel = NULL;
    cat_id = CAT_UNKNOWN;
    subcat_id = SUBCAT_UNKNOWN;
    psz_shortcut = NULL;
    b_loaded = false;
}

/* go over the config items and search text in psz_text
 * also search the node name and head */
bool PrefsItemData::contains( const QString &text, Qt::CaseSensitivity cs )
{
    if( this->i_type == TYPE_CATEGORY )
        return false;

    bool is_core = this->i_type != TYPE_MODULE;
    int id = this->subcat_id;

    /* find our module */
    module_t *p_module;
    if( !is_core )
        p_module = this->p_module;
    else
    {
        p_module = module_get_main();
        assert( p_module );
    }

    /* check the node itself (its name/longname/helptext) */

    QString head;
    if( is_core )
        head.clear();
    else
        head = QString( qfut( module_GetLongName( p_module ) ) );

    if ( name.contains( text, cs )
         || (!is_core && head.contains( text, cs ))
         || help.contains( text, cs )
       )
    {
        return true;
    }

    /* check options belonging to this core subcat or plugin */

    unsigned confsize;
    module_config_t *const p_config = module_config_get (p_module, &confsize),
                    *p_item = p_config,
                    *p_end = p_config + confsize;

    if( !p_config )
        return false;

    bool ret = false;

    if( is_core )
    {
        /* find start of relevant option block */
        while ( p_item < p_end )
        {
            if( p_item->i_type == CONFIG_SUBCATEGORY &&
                p_item->value.i == id
              )
                break;
            p_item++;
        }
        if( ++p_item >= p_end )
        {
            ret = false;
            goto end;
        }
    }

    do
    {
        if ( p_item->i_type == CONFIG_SUBCATEGORY )
        {
            /* for core, if we hit a subcat, stop */
            if ( is_core )
                break;
            /* a plugin's options are grouped under one node; we can/should
               ignore all other subcat entries. */
            continue;
        }

        /* cat-hint items are not relevant, they are an alternate set of headings for help output */
        if( p_item->i_type == CONFIG_HINT_CATEGORY ) continue;

        if ( p_item->psz_text && qfut( p_item->psz_text ).contains( text, cs ) )
        {
            ret = true;
            goto end;
        }
    }
    while ( ++p_item < p_end );

end:
    module_config_free( p_config );
    return ret;
}

/*********************************************************************
 * The Panel
 *********************************************************************/

AdvPrefsPanel::AdvPrefsPanel( qt_intf_t *_p_intf, QWidget *_parent,
                        PrefsItemData * data ) :
                        QWidget( _parent ), p_intf( _p_intf )
{
    /* Find our module */
    module_t *p_module = NULL;
    p_config = NULL;
    if( data->i_type == PrefsItemData::TYPE_CATEGORY )
        return;
    else if( data->i_type == PrefsItemData::TYPE_MODULE )
        p_module = data->p_module;
    else
    {
        p_module = module_get_main();
        assert( p_module );
    }

    unsigned confsize;
    p_config = module_config_get( p_module, &confsize );
    module_config_t *p_item = p_config,
                    *p_end = p_config + confsize;

    if( data->i_type == PrefsItemData::TYPE_SUBCATEGORY ||
        data->i_type == PrefsItemData::TYPE_CATSUBCAT )
    {
        while (p_item < p_end)
        {
            if(  p_item->i_type == CONFIG_SUBCATEGORY &&
                 p_item->value.i == data->subcat_id )
                break;
            p_item++;
        }
    }

    /* Widgets now */
    global_layout = new QVBoxLayout();
    global_layout->setMargin( 2 );
    QString head;
    QString help;

    help = QString( data->help );

    if( data->i_type == PrefsItemData::TYPE_SUBCATEGORY ||
        data->i_type == PrefsItemData::TYPE_CATSUBCAT )
    {
        head = QString( data->name );
        p_item++; // Why that ?
    }
    else
    {
        head = QString( qfut( module_GetLongName( p_module ) ) );
    }

    QLabel *titleLabel = new QLabel( head );
    QFont titleFont = QApplication::font();
    titleFont.setPointSize( titleFont.pointSize() + 6 );
    titleLabel->setFont( titleFont );

    // Title <hr>
    QFrame *title_line = new QFrame;
    title_line->setFrameShape(QFrame::HLine);
    title_line->setFrameShadow(QFrame::Sunken);

    QLabel *helpLabel = new QLabel( help, this );
    helpLabel->setWordWrap( true );

    global_layout->addWidget( titleLabel );
    global_layout->addWidget( title_line );
    global_layout->addWidget( helpLabel );

    QGroupBox *box = NULL;
    QGridLayout *boxlayout = NULL;

    QScrollArea *scroller= new QScrollArea;
    scroller->setFrameStyle( QFrame::NoFrame );
    QWidget *scrolled_area = new QWidget;

    QGridLayout *layout = new QGridLayout();
    int i_line = 0, i_boxline = 0;
    bool has_hotkey = false;

    if( p_item ) do
    {
        if( p_item->i_type == CONFIG_SUBCATEGORY &&
            ( data->i_type == PrefsItemData::TYPE_SUBCATEGORY ||
              data->i_type == PrefsItemData::TYPE_CATSUBCAT ) &&
            p_item->value.i != data->subcat_id )
            break;

        if( p_item->i_type == CONFIG_SECTION )
        {
            if( box && i_boxline > 0 )
            {
                box->setLayout( boxlayout );
                box->show();
                layout->addWidget( box, i_line, 0, 1, -1 );
                i_line++;
            }
            i_boxline = 0;
            box = new QGroupBox( qfut( p_item->psz_text ), this );
            box->hide();
            boxlayout = new QGridLayout();
        }
        /* Only one hotkey control */
        if( p_item->i_type == CONFIG_ITEM_KEY )
        {
            if( has_hotkey )
                continue;
            has_hotkey = true;
        }

        ConfigControl *control;
        if( ! box )
            control = ConfigControl::createControl( p_item, this,
                                                    layout, i_line );
        else
            control = ConfigControl::createControl( p_item, this,
                                                    boxlayout, i_boxline );
        if( !control )
            continue;

        if( box ) i_boxline++;
        else i_line++;
        controls.append( control );
    }
    while( !( ( data->i_type == PrefsItemData::TYPE_SUBCATEGORY ||
               data->i_type == PrefsItemData::TYPE_CATSUBCAT ) &&
             p_item->i_type == CONFIG_SUBCATEGORY )
        && ( ++p_item < p_end ) );

    if( box && i_boxline > 0 )
    {
        box->setLayout( boxlayout );
        box->show();
        layout->addWidget( box, i_line, 0, 1, -1 );
    }

    scrolled_area->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed );
    scrolled_area->setLayout( layout );
    scroller->setWidget( scrolled_area );
    scroller->setWidgetResizable( true );
    global_layout->addWidget( scroller );
    setLayout( global_layout );
}

void AdvPrefsPanel::apply()
{
    foreach ( ConfigControl *cfg, controls )
        cfg->doApply();
}

void AdvPrefsPanel::clean()
{}

AdvPrefsPanel::~AdvPrefsPanel()
{
    qDeleteAll( controls );
    controls.clear();
    module_config_free( p_config );
}
