/* This file is part of KDevelop
    Copyright 2006 Roberto Raggi <roberto@kdevelop.org>
    Copyright 2006-2008 Hamish Rodda <rodda@kde.org>
    Copyright 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "arraytype.h"

#include "../indexedstring.h"
#include "../repositories/typerepository.h"
#include "typesystemdata.h"
#include "typeregister.h"
#include "typesystem.h"

namespace KDevelop
{

REGISTER_TYPE(ArrayType);

ArrayType::ArrayType(const ArrayType& rhs) : AbstractType(copyData<ArrayTypeData>(*rhs.d_func())) {
}

ArrayType::ArrayType(ArrayTypeData& data) : AbstractType(data) {
}

AbstractType* ArrayType::clone() const {
  return new ArrayType(*this);
}

bool ArrayType::equals(const AbstractType* _rhs) const
{
  if( !fastCast<const ArrayType*>(_rhs))
    return false;
  const ArrayType* rhs = static_cast<const ArrayType*>(_rhs);
  TYPE_D(ArrayType);
  if( d->m_dimension != rhs->d_func()->m_dimension )
    return false;

  return d->m_elementType == rhs->d_func()->m_elementType;
}

ArrayType::ArrayType()
  : AbstractType(createData<ArrayTypeData>())
{
  d_func_dynamic()->setTypeClassId<ArrayType>();
}

ArrayType::~ArrayType()
{
}

int ArrayType::dimension () const
{
  return d_func()->m_dimension;
}

void ArrayType::setDimension(int dimension)
{
  kDebug() << "setting dimension" << dimension;
  d_func_dynamic()->m_dimension = dimension;
  kDebug() << "dimension" << d_func()->m_dimension;
}

AbstractType::Ptr ArrayType::elementType () const
{
  return d_func()->m_elementType.type();
}

void ArrayType::setElementType(AbstractType::Ptr type)
{
  d_func_dynamic()->m_elementType = type->indexed();
}

bool ArrayType::operator == (const ArrayType &other) const
{
  TYPE_D(ArrayType);
  return d->m_elementType == other.d_func()->m_elementType && d->m_dimension == other.d_func()->m_dimension;
}

bool ArrayType::operator != (const ArrayType &other) const
{
  TYPE_D(ArrayType);
  return d->m_elementType != other.d_func()->m_elementType || d->m_dimension != other.d_func()->m_dimension;
}

QString ArrayType::toString() const
{
  return QString("%1[%2]").arg(elementType() ? elementType()->toString() : QString("<notype>")).arg(d_func()->m_dimension);
}

void ArrayType::accept0 (TypeVisitor *v) const
{
  if (v->visit (this))
    {
      acceptType (d_func()->m_elementType.type(), v);
    }

  v->endVisit (this);
}

void ArrayType::exchangeTypes( TypeExchanger* exchanger )
{
  TYPE_D_DYNAMIC(ArrayType);
  d->m_elementType = exchanger->exchange( d->m_elementType.type() )->indexed();
}

AbstractType::WhichType ArrayType::whichType() const
{
  return TypeArray;
}

uint ArrayType::hash() const
{
  return (elementType() ? elementType()->hash() : 0) * 47 + 117* dimension();
}

}

// kate: space-indent on; indent-width 2; tab-width 4; replace-tabs on; auto-insert-doxygen on
