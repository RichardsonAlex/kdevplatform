#ifndef KROSSVCSREVISION_H
#define KROSSVCSREVISION_H

#include<QtCore/QVariant>

//This is file has been generated by xmltokross, you should not edit this file but the files used to generate it.

#include <vcs/vcsrevision.h>
namespace Handlers
{
	QVariant _vcsRevisionHandler(void* type);
	QVariant vcsRevisionHandler(KDevelop::VcsRevision* type) { return _vcsRevisionHandler((void*) type); }
	QVariant vcsRevisionHandler(const KDevelop::VcsRevision* type) { return _vcsRevisionHandler((void*) type); }

}

#endif
