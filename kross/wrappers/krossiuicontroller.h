#ifndef KROSSIUICONTROLLER_H
#define KROSSIUICONTROLLER_H

#include<QtCore/QVariant>

//This is file has been generated by xmltokross, you should not edit this file but the files used to generate it.

#include <interfaces/iuicontroller.h>
namespace Handlers
{
	QVariant _iToolViewFactoryHandler(void* type);
	QVariant iToolViewFactoryHandler(KDevelop::IToolViewFactory* type) { return _iToolViewFactoryHandler((void*) type); }
	QVariant iToolViewFactoryHandler(const KDevelop::IToolViewFactory* type) { return _iToolViewFactoryHandler((void*) type); }

	QVariant _iUiControllerHandler(void* type);
	QVariant iUiControllerHandler(KDevelop::IUiController* type) { return _iUiControllerHandler((void*) type); }
	QVariant iUiControllerHandler(const KDevelop::IUiController* type) { return _iUiControllerHandler((void*) type); }

}

#endif
