/*
 *
 *                This source code is part of
 *
 *                 G   R   O   M   A   C   S
 *
 *          GROningen MAchine for Chemical Simulations
 *
 * Written by David van der Spoel, Erik Lindahl, Berk Hess, and others.
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2009, The GROMACS development team,
 * check out http://www.gromacs.org for more information.

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * If you want to redistribute modifications, please consider that
 * scientific software is very special. Version control is crucial -
 * bugs must be traceable. We will be happy to consider code for
 * inclusion in the official distribution, but derived work must not
 * be called official GROMACS. Details are found in the README & COPYING
 * files - if they are missing, get the official version at www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the papers on the package - you can find them in the top README file.
 *
 * For more info, check our website at http://www.gromacs.org
 */
/*! \internal \file
 * \brief
 * Implements classes in selectionoption.h and selectionoptionstorage.h.
 *
 * \author Teemu Murtola <teemu.murtola@cbr.su.se>
 * \ingroup module_selection
 */
#include "gromacs/selection/selectionoption.h"

#include <string>
#include <vector>

#include "gromacs/fatalerror/exceptions.h"
#include "gromacs/fatalerror/gmxassert.h"
#include "gromacs/fatalerror/messagestringcollector.h"
#include "gromacs/options/options.h"
#include "gromacs/options/optionsvisitor.h"
#include "gromacs/selection/selection.h"
#include "gromacs/selection/selectioncollection.h"
#include "gromacs/selection/selectionoptioninfo.h"

#include "selectioncollection-impl.h"
#include "selectionoptionstorage.h"

namespace gmx
{

/********************************************************************
 * SelectionOptionStorage
 */

SelectionOptionStorage::SelectionOptionStorage(const SelectionOption &settings,
                                               Options *options)
    : MyBase(settings, options,
             OptionFlags() | efNoDefaultValue | efDontCheckMinimumCount),
      _info(this), _sc(NULL), _selectionFlags(settings._selectionFlags)
{
    if (settings._infoPtr != NULL)
    {
        *settings._infoPtr = &_info;
    }
}


std::string SelectionOptionStorage::formatValue(int i) const
{
    Selection *sel = values().at(i);
    return (sel != NULL ? sel->selectionText() : "");
}


void SelectionOptionStorage::addSelections(
        const std::vector<Selection *> &selections,
        bool bFullValue)
{
    if (bFullValue && selections.size() < static_cast<size_t>(minValueCount()))
    {
        GMX_THROW(InvalidInputError("Too few selections provided"));
    }
    if (bFullValue)
    {
        clearSet();
    }
    std::vector<Selection *>::const_iterator i;
    for (i = selections.begin(); i != selections.end(); ++i)
    {
        // TODO: Having this check in the parser would make interactive input
        // behave better.
        if (_selectionFlags.test(efOnlyStatic) && (*i)->isDynamic())
        {
            GMX_THROW(InvalidInputError("Dynamic selections not supported"));
        }
        (*i)->setFlags(_selectionFlags);
        addValue(*i);
    }
    if (bFullValue)
    {
        commitValues();
    }
}


void SelectionOptionStorage::convertValue(const std::string &value)
{
    GMX_RELEASE_ASSERT(_sc != NULL, "Selection collection is not set");

    std::vector<Selection *> selections;
    // TODO: Implement reading from a file.
    _sc->parseFromString(value, &selections);
    addSelections(selections, false);
}

void SelectionOptionStorage::processSetValues(ValueList *values)
{
    if (values->size() > 0 && values->size() < static_cast<size_t>(minValueCount()))
    {
        GMX_THROW(InvalidInputError("Too few (valid) values provided"));
    }
}

void SelectionOptionStorage::processAll()
{
    if ((hasFlag(efRequired) || hasFlag(efSet)) && valueCount() == 0)
    {
        GMX_RELEASE_ASSERT(_sc != NULL, "Selection collection is not set");

        _sc->_impl->requestSelections(name(), description(), this);
        setFlag(efSet);
    }
}

void SelectionOptionStorage::setAllowedValueCount(int count)
{
    MessageStringCollector errors;
    errors.startContext("In option '" + name() + "'");
    if (count >= 0)
    {
        // Should not throw because efDontCheckMinimumCount is set
        setMinValueCount(count);
        if (valueCount() > 0 && valueCount() < count)
        {
            errors.append("Too few (valid) values provided");
        }
    }
    try
    {
        setMaxValueCount(count);
    }
    catch (const UserInputError &ex)
    {
        errors.append(ex.what());
    }
    errors.finishContext();
    if (!errors.isEmpty())
    {
        GMX_THROW(InvalidInputError(errors.toString()));
    }
}

void SelectionOptionStorage::setSelectionFlag(SelectionFlag flag, bool bSet)
{
    _selectionFlags.set(flag, bSet);
    ValueList::const_iterator i;
    for (i = values().begin(); i != values().end(); ++i)
    {
        if (_selectionFlags.test(efOnlyStatic) && (*i)->isDynamic())
        {
            MessageStringCollector errors;
            errors.startContext("In option '" + name() + "'");
            errors.append("Dynamic selections not supported");
            errors.finishContext();
            GMX_THROW(InvalidInputError(errors.toString()));
        }
        (*i)->setFlags(_selectionFlags);
    }
}


/********************************************************************
 * SelectionOptionInfo
 */

SelectionOptionInfo::SelectionOptionInfo(SelectionOptionStorage *option)
    : OptionInfo(option)
{
}

SelectionOptionStorage &SelectionOptionInfo::option()
{
    return static_cast<SelectionOptionStorage &>(OptionInfo::option());
}

const SelectionOptionStorage &SelectionOptionInfo::option() const
{
    return static_cast<const SelectionOptionStorage &>(OptionInfo::option());
}

void SelectionOptionInfo::setSelectionCollection(SelectionCollection *selections)
{
    option().setSelectionCollection(selections);
}

void SelectionOptionInfo::setValueCount(int count)
{
    option().setAllowedValueCount(count);
}

void SelectionOptionInfo::setEvaluateVelocities(bool bEnabled)
{
    option().setSelectionFlag(efEvaluateVelocities, bEnabled);
}

void SelectionOptionInfo::setEvaluateForces(bool bEnabled)
{
    option().setSelectionFlag(efEvaluateForces, bEnabled);
}

void SelectionOptionInfo::setOnlyAtoms(bool bEnabled)
{
    option().setSelectionFlag(efOnlyAtoms, bEnabled);
}

void SelectionOptionInfo::setOnlyStatic(bool bEnabled)
{
    option().setSelectionFlag(efOnlyStatic, bEnabled);
}

void SelectionOptionInfo::setDynamicMask(bool bEnabled)
{
    option().setSelectionFlag(efDynamicMask, bEnabled);
}

void SelectionOptionInfo::setDynamicOnlyWhole(bool bEnabled)
{
    option().setSelectionFlag(efDynamicOnlyWhole, bEnabled);
}


/********************************************************************
 * SelectionOption
 */

AbstractOptionStorage *SelectionOption::createDefaultStorage(Options *options) const
{
    return new SelectionOptionStorage(*this, options);
}


/********************************************************************
 * Global functions
 */

namespace
{

/*! \internal \brief
 * Visitor that sets the selection collection for each selection option.
 *
 * \ingroup module_selection
 */
class SelectionCollectionSetter : public OptionsModifyingTypeVisitor<SelectionOptionInfo>
{
    public:
        //! Construct a visitor that sets given selection collection.
        explicit SelectionCollectionSetter(SelectionCollection *selections)
            : selections_(selections)
        {
        }

        void visitSubSection(Options *section)
        {
            OptionsModifyingIterator iterator(section);
            iterator.acceptSubSections(this);
            iterator.acceptOptions(this);
        }

        void visitOptionType(SelectionOptionInfo *option)
        {
            option->setSelectionCollection(selections_);
        }

    private:
        SelectionCollection    *selections_;
};

} // namespace

void setSelectionCollectionForOptions(Options *options,
                                      SelectionCollection *selections)
{
    SelectionCollectionSetter(selections).visitSubSection(options);
}

} // namespace gmx