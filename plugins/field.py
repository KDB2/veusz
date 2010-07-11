#    Copyright (C) 2010 Jeremy S. Sanders
#    Email: Jeremy Sanders <jeremy@jeremysanders.net>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
##############################################################################

# $Id$

"""Data entry fields for plugins."""

import veusz.qtall as qt4
import veusz.utils as utils
import veusz.setting as setting

class Field(object):
    """A class to represent an input field on the dialog or command line."""
    def __init__(self, name, descr=None, default=None):
        """name: name of field
        descr: description to show to user
        default: default value."""
        self.name = name
        if descr:
            self.descr = descr
        else:
            self.descr = name
        self.default = default

    def makeControl(self, doc=None):
        """Create a set of controls for field."""
        return None

    def getControlResults(self, cntrls):
        """Get result from created contrls."""
        return None

class FieldText(Field):
    """Text entry on the dialog."""

    def makeControl(self, doc=None):
        l = qt4.QLabel(self.descr)
        e = qt4.QLineEdit()
        if self.default:
            e.setText(self.default)
        return (l, e)

    def getControlResults(self, cntrls):
        return unicode( cntrls[1].text() )

class FieldCombo(Field):
    """Drop-down combobox on dialog."""
    def __init__(self, name, descr=None, default=None, items=(),
                 editable=True):
        """name: name of field
        descr: description to show to user
        default: default value
        items: items in drop-down box
        editable: whether user can enter their own value."""
        Field.__init__(self, name, descr=descr, default=default)
        self.items = items
        self.editable = editable

    def makeControl(self, doc=None):
        l = qt4.QLabel(self.descr)
        c = qt4.QComboBox()
        c.addItems(self.items)
        c.setEditable(bool(self.editable))

        if self.default:
            if self.editable:
                c.setEditText(self.default)
            else:
                c.setCurrentIndex(c.findText(self.default))

        return (l, c)

    def getControlResults(self, cntrls):
        return unicode( cntrls[1].currentText() )

class _WidgetCombo(qt4.QComboBox):
    """Combo box for selecting widgets."""

    def __init__(self, doc, widgettypes, default):
        """doc: Veusz document
        widgettypes: set of allowed widgettypes or empty for all
        default: default path."""

        qt4.QComboBox.__init__(self)
        self.doc = doc
        self.widgettypes = widgettypes
        self.default = default
        self.updateWidgets()
        self.connect(doc, qt4.SIGNAL("sigModified"), self.updateWidgets)

    def updateWidgets(self):
        """Update combo with new widgets."""

        self.paths = []    # veusz widget paths of items
        comboitems = []    # names of items (with tree spacing)

        def iterateWidgets(widget, level):
            """Walk tree recursively."""
            if not self.widgettypes or widget.typename in self.widgettypes:
                comboitems.append('  '*level + widget.name)
                self.paths.append(widget.path)
            for w in widget.children:
                iterateWidgets(w, level+1)

        iterateWidgets(self.doc.basewidget, 0)
        if self.count() == 0:
            # first time around add default to get it selected, yuck :-(
            try:
                idx = self.paths.index(self.default)
                self.addItem( comboitems[idx] )
            except ValueError:
                pass

        utils.populateCombo(self, comboitems)
    
    def getWidgetPath(self):
        """Get path of selected widget."""
        return self.paths[self.currentIndex()]

class FieldWidget(Field):
    """Drop-down combobox for selecting widgets."""

    def __init__(self, name, descr=None, default='/', widgettypes=set()):
        """name: name of field
        descr: description to show to user
        default: default value."""
        Field.__init__(self, name, descr=descr, default=default)
        self.widgettypes = widgettypes

    def makeControl(self, doc=None):
        l = qt4.QLabel(self.descr)
        c = _WidgetCombo(doc, self.widgettypes, self.default)
        return (l, c)

    def getControlResults(self, cntrls):
        return cntrls[1].getWidgetPath()

class _FieldSetting(Field):
    """Field using a setting internally to avoid code duplication.
    Designed to be subclassed."""

    def __init__(self, settingkls, name, descr=None, default='',
                 setnparams = {}):
        Field.__init__(self, name, descr=descr, default=default)
        self.default = default
        self.setn = settingkls(name, default, **setnparams)

    def makeControl(self, doc=None):
        """Use setting makeControl method to make control."""
        self.setn.parent = self # setting looks to parent for document
        self.setn.set(self.default)

        self.document = doc
        l = qt4.QLabel(self.descr)
        c = self.setn.makeControl(None)

        def updateval(cntrl, setn, val):
            setn.set(val)

        # if control changes setting, update setting
        c.connect(c, qt4.SIGNAL('settingChanged'), updateval)

        return (l, c)

    def getControlResults(self, cntrls):
        """Get result from setting."""
        return self.setn.get()

class FieldBool(_FieldSetting):
    """A true/false value using a check box."""
    def __init__(self, name, descr=None, default=False):
        _FieldSetting.__init__(self, setting.Bool, name,
                               descr=descr, default=default)

class FieldInt(_FieldSetting):
    """An integer number field."""

    def __init__(self, name, descr=None, default=0,
                 minval=-9999999, maxval=9999999):
        """name: name of field
        descr: description to show to user
        default: default value.
        minval and maxval: minimum and maximum integers
        """
        _FieldSetting.__init__(self, setting.Int,
                               name, descr=descr, default=default,
                               setnparams={'minval': minval, 'maxval': maxval})

class FieldFloat(_FieldSetting):
    """A floating point number field."""

    def __init__(self, name, descr=None, default=None,
                 minval=-1e99, maxval=1e99):
        """name: name of field
        descr: description to show to user
        default: default value.
        minval and maxval: minimum and maximum values
        """

        _FieldSetting.__init__(self, setting.Float,
                               name, descr=descr, default=default,
                               setnparams={'minval': minval, 'maxval': maxval})

class FieldColor(_FieldSetting):
    """Field for selecting a color - returns #rrggbb string."""
    def __init__(self, name, descr=None, default='black'):
        _FieldSetting.__init__(self, setting.Color, name,
                               descr=descr, default=default)

class FieldFillStyle(_FieldSetting):
    """Field for selecting fill styles - returns a string."""
    def __init__(self, name, descr=None, default='solid'):
        _FieldSetting.__init__(self, setting.FillStyle, name,
                               descr=descr, default=default)

class FieldLineStyle(_FieldSetting):
    """Field for selecting line styles - returns a string."""
    def __init__(self, name, descr=None, default='solid'):
        _FieldSetting.__init__(self, setting.LineStyle, name,
                               descr=descr, default=default)

class FieldMarker(_FieldSetting):
    """Field for selecting a marker type.
    
    Returns a string
    """
    def __init__(self, name, descr=None, default='circle'):
        _FieldSetting.__init__(self, setting.Marker, name,
                               descr=descr, default=default)

class FieldArrow(_FieldSetting):
    """Field for selecting an arrow type.
    
    Returns a string
    """
    def __init__(self, name, descr=None, default='none'):
        _FieldSetting.__init__(self, setting.Arrow, name,
                               descr=descr, default=default)

class FieldErrorStyle(_FieldSetting):
    """Field for selecting an error bar style
    
    Returns a string
    """
    def __init__(self, name, descr=None, default='bar'):
        _FieldSetting.__init__(self, setting.ErrorStyle, name,
                               descr=descr, default=default)

class FieldDistance(_FieldSetting):
    """Field for selecting a veusz-style distance, e.g. '1pt'.

    Returns a string
    """
    def __init__(self, name, descr=None, default='1pt'):
        _FieldSetting.__init__(self, setting.Distance, name,
                               descr=descr, default=default)

class FieldFloatList(_FieldSetting):
    """Field for entering multiple numbers, separated by commas or spaces

    Returns a list/tuple of floats
    """
    def __init__(self, name, descr=None, default=()):
        _FieldSetting.__init__(self, setting.FloatList, name,
                               descr=descr, default=default)

class FieldDataset(_FieldSetting):
    """Field for selecting a datset.
    Returns a string.

    Note that the validity of dataset names is not checked
    Note that a blank string may result
    """

    def __init__(self, name, descr=None, default='', dims=1,
                 datatype='numeric'):
        """name: name of field
        descr: description to show to user
        default: default value (ignored currently)
        dims: dimensions of dataset to show
        datatype: type of data: numeric or text
        """
        _FieldSetting.__init__(self, setting.Dataset,
                               name, descr=descr, default=default,
                               setnparams={'dimensions': dims,
                                           'datatype': datatype})

class FieldTextMulti(_FieldSetting):
    """Field for entering multiple lines of text.
    Returns a tuple/list of strings.
    """
    def __init__(self, name, descr=None, default=('')):
        _FieldSetting.__init__(self, setting.Strings, name,
                               descr=descr, default=default)

class FieldDatasetMulti(_FieldSetting):
    """Field for entering multiple datasets.
    Returns a tuple/list of strings.
    """
    def __init__(self, name, descr=None, default=(''), dims=1,
                 datatype='numeric'):
        """dims is number of dimensions of datasets to show in
        drop-down list.

        datatype is 'numeric' or 'text'
        """
        _FieldSetting.__init__(self, setting.Datasets, name,
                               descr=descr, default=default,
                               setnparams={'dimensions': dims,
                                           'datatype': datatype})

class FieldLineMulti(_FieldSetting):
    """A field for holding a set of lines. Consists of tuples
    [('dotted', '1pt', 'color', <trans>, False), ...]

    These are style, width, color, and hide or
    style, widget, color, transparency, hide

    This is compatible with the contour widget line style
    """

    def __init__(self, name, descr=None,
                 default=(('solid', '1pt', 'black', False),) ):
        _FieldSetting.__init__(self, setting.LineSet, name,
                               descr=descr, default=default)

class FieldFillMulti(_FieldSetting):
    """A field for holding a set of fills. Consists of tuples

    [('solid', 'color', <trans>, False), ...]
        
    These are color, fill style, and hide or
    color, fill style, transparency and hide

    This is compatible with the contour widget line style
    """

    def __init__(self, name, descr=None, default=()):
        _FieldSetting.__init__(self, setting.FillSet, name,
                               descr=descr, default=default)

class FieldFontFamily(_FieldSetting):
    """A field for holding a font family.

    Returns a string.
    """

    def __init__(self, name, descr=None, default=None):
        """Default None selects the default font."""
        if default is None:
            default = setting.Text.defaultfamily
        _FieldSetting.__init__(self, setting.FontFamily, name,
                               descr=descr, default=default)

class FieldFilename(_FieldSetting):
    """Select a filename with a browse button."""

    def __init__(self, name, descr=None, default=''):
        _FieldSetting.__init__(self, setting.Filename, name,
                               descr=descr, default=default)
