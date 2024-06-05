/****************************************************************************
** Meta object code from reading C++ file 'MilesEdgeworth.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../MilesEdgeworth.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MilesEdgeworth.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.5.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSMilesEdgeworthENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSMilesEdgeworthENDCLASS = QtMocHelpers::stringData(
    "MilesEdgeworth",
    "soundPlay",
    "",
    "soundStop",
    "exitProgram",
    "on_viewerClosing",
    "prevIsNull",
    "PicViewer*",
    "nextPtr",
    "on_trayActivated",
    "QSystemTrayIcon::ActivationReason",
    "reason"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSMilesEdgeworthENDCLASS_t {
    uint offsetsAndSizes[24];
    char stringdata0[15];
    char stringdata1[10];
    char stringdata2[1];
    char stringdata3[10];
    char stringdata4[12];
    char stringdata5[17];
    char stringdata6[11];
    char stringdata7[11];
    char stringdata8[8];
    char stringdata9[17];
    char stringdata10[34];
    char stringdata11[7];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSMilesEdgeworthENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSMilesEdgeworthENDCLASS_t qt_meta_stringdata_CLASSMilesEdgeworthENDCLASS = {
    {
        QT_MOC_LITERAL(0, 14),  // "MilesEdgeworth"
        QT_MOC_LITERAL(15, 9),  // "soundPlay"
        QT_MOC_LITERAL(25, 0),  // ""
        QT_MOC_LITERAL(26, 9),  // "soundStop"
        QT_MOC_LITERAL(36, 11),  // "exitProgram"
        QT_MOC_LITERAL(48, 16),  // "on_viewerClosing"
        QT_MOC_LITERAL(65, 10),  // "prevIsNull"
        QT_MOC_LITERAL(76, 10),  // "PicViewer*"
        QT_MOC_LITERAL(87, 7),  // "nextPtr"
        QT_MOC_LITERAL(95, 16),  // "on_trayActivated"
        QT_MOC_LITERAL(112, 33),  // "QSystemTrayIcon::ActivationRe..."
        QT_MOC_LITERAL(146, 6)   // "reason"
    },
    "MilesEdgeworth",
    "soundPlay",
    "",
    "soundStop",
    "exitProgram",
    "on_viewerClosing",
    "prevIsNull",
    "PicViewer*",
    "nextPtr",
    "on_trayActivated",
    "QSystemTrayIcon::ActivationReason",
    "reason"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSMilesEdgeworthENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   44,    2, 0x06,    1 /* Public */,
       3,    0,   45,    2, 0x06,    2 /* Public */,
       4,    0,   46,    2, 0x06,    3 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       5,    2,   47,    2, 0x0a,    4 /* Public */,
       9,    1,   52,    2, 0x0a,    7 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::Bool, 0x80000000 | 7,    6,    8,
    QMetaType::Void, 0x80000000 | 10,   11,

       0        // eod
};

Q_CONSTINIT const QMetaObject MilesEdgeworth::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_CLASSMilesEdgeworthENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSMilesEdgeworthENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSMilesEdgeworthENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MilesEdgeworth, std::true_type>,
        // method 'soundPlay'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'soundStop'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'exitProgram'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'on_viewerClosing'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<PicViewer *, std::false_type>,
        // method 'on_trayActivated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QSystemTrayIcon::ActivationReason, std::false_type>
    >,
    nullptr
} };

void MilesEdgeworth::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MilesEdgeworth *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->soundPlay(); break;
        case 1: _t->soundStop(); break;
        case 2: _t->exitProgram(); break;
        case 3: _t->on_viewerClosing((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<PicViewer*>>(_a[2]))); break;
        case 4: _t->on_trayActivated((*reinterpret_cast< std::add_pointer_t<QSystemTrayIcon::ActivationReason>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< PicViewer* >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (MilesEdgeworth::*)();
            if (_t _q_method = &MilesEdgeworth::soundPlay; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (MilesEdgeworth::*)();
            if (_t _q_method = &MilesEdgeworth::soundStop; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (MilesEdgeworth::*)();
            if (_t _q_method = &MilesEdgeworth::exitProgram; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
    }
}

const QMetaObject *MilesEdgeworth::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MilesEdgeworth::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSMilesEdgeworthENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int MilesEdgeworth::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void MilesEdgeworth::soundPlay()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void MilesEdgeworth::soundStop()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void MilesEdgeworth::exitProgram()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
