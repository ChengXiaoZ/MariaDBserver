/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
#include "my_dir.h"

/*****************************************************************************
** Item functions
*****************************************************************************/

/* Init all special items */

void item_init(void)
{
  item_user_lock_init();
}

Item::Item():
  fixed(0)
{
  marker=0;
  maybe_null=null_value=with_sum_func=unsigned_flag=0;
  name=0;
  decimals=0; max_length=0;
  next=current_thd->free_list;			// Put in free list
  current_thd->free_list=this;
  loop_id= 0;
}

bool Item::check_loop(uint id)
{
  DBUG_ENTER("Item::check_loop");
  DBUG_PRINT("info", ("id %u, name %s", id, name));
  if (loop_id == id)
  {
    DBUG_PRINT("info", ("id match"));
    DBUG_RETURN(1);
  }
  loop_id= id;
  DBUG_RETURN(0);
}

bool Item::check_cols(uint c)
{
  if (c != 1)
  {
    my_error(ER_CARDINALITY_COL, MYF(0), c);
    return 1;
  }
  return 0;
}

void Item::set_name(const char *str,uint length)
{
  if (!length)
    name= (char*) str;				// Used by AS
  else
  {
    while (length && !my_isgraph(system_charset_info,*str))
    {						// Fix problem with yacc
      length--;
      str++;
    }
    name=sql_strmake(str,min(length,MAX_FIELD_WIDTH));
  }
}

/*
  This function is only called when comparing items in the WHERE clause
*/

bool Item::eq(const Item *item, bool binary_cmp) const
{
  return type() == item->type() && name && item->name &&
    !my_strcasecmp(system_charset_info,name,item->name);
}

bool Item_string::eq(const Item *item, bool binary_cmp) const
{
  if (type() == item->type())
  {
    if (binary_cmp)
      return !stringcmp(&str_value, &item->str_value);
    return !sortcmp(&str_value, &item->str_value);
  }
  return 0;
}


/*
  Get the value of the function as a TIME structure.
  As a extra convenience the time structure is reset on error!
 */

bool Item::get_date(TIME *ltime,bool fuzzydate)
{
  char buff[40];
  String tmp(buff,sizeof(buff),default_charset_info),*res;
  if (!(res=val_str(&tmp)) ||
      str_to_TIME(res->ptr(),res->length(),ltime,fuzzydate) == TIMESTAMP_NONE)
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

/*
  Get time of first argument.
  As a extra convenience the time structure is reset on error!
 */

bool Item::get_time(TIME *ltime)
{
  char buff[40];
  String tmp(buff,sizeof(buff),default_charset_info),*res;
  if (!(res=val_str(&tmp)) ||
      str_to_time(res->ptr(),res->length(),ltime))
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

CHARSET_INFO * Item::thd_charset() const
{
  return current_thd->thd_charset;
}

Item_field::Item_field(Field *f) :Item_ident(NullS,f->table_name,f->field_name)
{
  set_field(f);
  fixed= 1; // This item is not needed in fix_fields
}


void Item_field::set_field(Field *field_par)
{
  field=result_field=field_par;			// for easy coding with fields
  maybe_null=field->maybe_null();
  max_length=field_par->field_length;
  decimals= field->decimals();
  table_name=field_par->table_name;
  field_name=field_par->field_name;
  unsigned_flag=test(field_par->flags & UNSIGNED_FLAG);
  set_charset(field_par->charset());
}

const char *Item_ident::full_name() const
{
  char *tmp;
  if (!table_name)
    return field_name ? field_name : name ? name : "tmp_field";
  if (db_name)
  {
    tmp=(char*) sql_alloc((uint) strlen(db_name)+(uint) strlen(table_name)+
			  (uint) strlen(field_name)+3);
    strxmov(tmp,db_name,".",table_name,".",field_name,NullS);
  }
  else
  {
    tmp=(char*) sql_alloc((uint) strlen(table_name)+
			  (uint) strlen(field_name)+2);
    strxmov(tmp,table_name,".",field_name,NullS);
  }
  return tmp;
}

/* ARGSUSED */
String *Item_field::val_str(String *str)
{
  if ((null_value=field->is_null()))
    return 0;
  return field->val_str(str,&str_value);
}

double Item_field::val()
{
  if ((null_value=field->is_null()))
    return 0.0;
  return field->val_real();
}

longlong Item_field::val_int()
{
  if ((null_value=field->is_null()))
    return 0;
  return field->val_int();
}


String *Item_field::str_result(String *str)
{
  if ((null_value=result_field->is_null()))
    return 0;
  return result_field->val_str(str,&str_value);
}

bool Item_field::get_date(TIME *ltime,bool fuzzydate)
{
  if ((null_value=field->is_null()) || field->get_date(ltime,fuzzydate))
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

bool Item_field::get_time(TIME *ltime)
{
  if ((null_value=field->is_null()) || field->get_time(ltime))
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

double Item_field::val_result()
{
  if ((null_value=result_field->is_null()))
    return 0.0;
  return result_field->val_real();
}

longlong Item_field::val_int_result()
{
  if ((null_value=result_field->is_null()))
    return 0;
  return result_field->val_int();
}

bool Item_field::eq(const Item *item, bool binary_cmp) const
{
  return item->type() == FIELD_ITEM && ((Item_field*) item)->field == field;
}

table_map Item_field::used_tables() const
{
  if (field->table->const_table)
    return 0;					// const item
  return field->table->map;
}


String *Item_int::val_str(String *str)
{
  str->set(value, thd_charset());
  return str;
}

void Item_int::print(String *str)
{
  if (!name)
  {
    str_value.set(value, thd_charset());
    name=str_value.c_ptr();
  }
  str->append(name);
}

String *Item_uint::val_str(String *str)
{
  str->set((ulonglong) value, thd_charset());
  return str;
}

void Item_uint::print(String *str)
{
  if (!name)
  {
    str_value.set((ulonglong) value, thd_charset());
    name=str_value.c_ptr();
  }
  str->append(name);
}


String *Item_real::val_str(String *str)
{
  str->set(value,decimals,thd_charset());
  return str;
}

void Item_string::print(String *str)
{
  str->append('\'');
  str->append(full_name());
  str->append('\'');
}

bool Item_null::eq(const Item *item, bool binary_cmp) const
{ return item->type() == type(); }
double Item_null::val() { null_value=1; return 0.0; }
longlong Item_null::val_int() { null_value=1; return 0; }
/* ARGSUSED */
String *Item_null::val_str(String *str)
{ null_value=1; return 0;}


/* Item_param related */
void Item_param::set_null()
{ 
  maybe_null=null_value=1;    
}

void Item_param::set_int(longlong i)
{  
  int_value=(longlong)i; 
  item_type = INT_ITEM;
}

void Item_param::set_double(double value)
{  
  real_value=value;
  item_type = REAL_ITEM;
}


void Item_param::set_value(const char *str, uint length)
{  
  str_value.set(str,length,thd_charset());
  item_type = STRING_ITEM;
}


void Item_param::set_longdata(const char *str, ulong length)
{  
  str_value.append(str,length);
  long_data_supplied= 1;
}


int Item_param::save_in_field(Field *field, bool no_conversions)
{
  if (null_value)
    return (int) set_field_to_null(field);   
    
  field->set_notnull();
  if (item_result_type == INT_RESULT)
  {
    longlong nr=val_int();
    return (field->store(nr)) ? -1 : 0;
  }
  if (item_result_type == REAL_RESULT)
  {
    double nr=val();    
    return (field->store(nr)) ? -1 : 0; 
  }
  String *result=val_str(&str_value);
  return (field->store(result->ptr(),result->length(),field->charset())) ? -1 : 0;
}


void Item_param::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_STRING);
}


double Item_param::val() 
{
  switch (item_result_type) {
  case STRING_RESULT:
    return (double)atof(str_value.ptr()); 
  case INT_RESULT:
    return (double)int_value;
  default:
    return real_value;
  }
} 


longlong Item_param::val_int() 
{ 
 switch (item_result_type) {
  case STRING_RESULT:
    return strtoll(str_value.ptr(),(char**) 0,10);
  case REAL_RESULT:
    return (longlong) (real_value+(real_value > 0 ? 0.5 : -0.5));
  default:
    return int_value;
  }
}


String *Item_param::val_str(String* str) 
{ 
  switch (item_result_type) {
  case INT_RESULT:
    str->set(int_value, thd_charset());
    return str;
  case REAL_RESULT:
    str->set(real_value, 2, thd_charset());
    return str;
  default:
    return (String*) &str_value;
  }
}
/* End of Item_param related */


void Item_copy_string::copy()
{
  String *res=item->val_str(&str_value);
  if (res && res != &str_value)
    str_value.copy(*res);
  null_value=item->null_value;
}

/* ARGSUSED */
String *Item_copy_string::val_str(String *str)
{
  if (null_value)
    return (String*) 0;
  return &str_value;
}

/*
  Functions to convert item to field (for send_fields)
*/

/* ARGSUSED */
bool Item::fix_fields(THD *thd,
		      struct st_table_list *list,
		      Item ** ref)
{
  fixed= 1;
  return 0;
}

bool Item_outer_select_context_saver::fix_fields(THD *thd,
						 struct st_table_list *list,
						 Item ** ref)
{
  DBUG_ENTER("Item_outer_select_context_saver::fix_fields");
  bool res= item->fix_fields(thd,
			     0, // do not show current subselect fields
			     &item);
  *ref= item;
  DBUG_RETURN(res);
}

bool Item_asterisk_remover::fix_fields(THD *thd,
				       struct st_table_list *list,
				       Item ** ref)
{
  DBUG_ENTER("Item_asterisk_remover::fix_fields");
  
  bool res= 1;
  if (item)
    if (item->type() == Item::FIELD_ITEM &&
	((Item_field*) item)->field_name[0] == '*')
    {
      Item_field *fitem=  (Item_field*) item;
      if (list)
	if (!list->next || fitem->db_name || fitem->table_name)
	{
	  TABLE_LIST *table= find_table_in_list(list,
						fitem->db_name,
						fitem->table_name);
	  if (table)
	  {
	    TABLE * tb= table->table;
	    if (find_table_in_list(table->next, fitem->db_name,
				   fitem->table_name) != 0 ||
		tb->fields == 1)
	    {
	      if ((item= new Item_field(tb->field[0])))
	      {
		res= 0;
		tb->field[0]->query_id= thd->query_id;
		tb->used_keys&= tb->field[0]->part_of_key;
		tb->used_fields= tb->fields;
	      }
	      else
		thd->fatal_error= 1; // can't create Item => out of memory
	    }
	    else
	      my_error(ER_CARDINALITY_COL, MYF(0), 1);
	  }
	  else
	    my_error(ER_BAD_TABLE_ERROR, MYF(0), fitem->table_name);
	}
	else
	  my_error(ER_CARDINALITY_COL, MYF(0), 1);
      else
	my_error(ER_NO_TABLES_USED, MYF(0));
    }   
    else
      res= item->fix_fields(thd, list, &item);
  else
    thd->fatal_error= 1; // no item given => out of memory
  *ref= item;
  DBUG_RETURN(res);
}


bool Item_field::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  if (!field)					// If field is not checked
  {
    Field *tmp;
    if ((tmp= find_field_in_tables(thd, this, tables, 0)) == not_found_field)
    {
      /*
	We can't find table field in table list of current select, 
	consequently we have to find it in outer subselect(s).
	We can't join lists of outer & current select, because of scope 
	of view rules. For example if both tables (outer & current) have 
	field 'field' it is not mistake to refer to this field without 
	mention of table name, but if we join tables in one list it will
	cause error ER_NON_UNIQ_ERROR in find_field_in_tables.
      */
      SELECT_LEX *last= 0;
      
      Item **refer= (Item **)not_found_item;
      // Prevent using outer fields in subselects, that is not supported now
      SELECT_LEX *cursel=(SELECT_LEX *) thd->lex.current_select;
      if (cursel->linkage != DERIVED_TABLE_TYPE)
	for (SELECT_LEX *sl=cursel->outer_select();
	     sl;
	     sl= sl->outer_select())
	{
	  if ((tmp= find_field_in_tables(thd, this,
					 (last= sl)->get_table_list(),
					 0)) != not_found_field)
	    break;
	  if ((refer= find_item_in_list(this, sl->item_list,
				       REPORT_EXCEPT_NOT_FOUND)) !=
	     (Item **)not_found_item)
	    break;
	  if (sl->linkage == DERIVED_TABLE_TYPE)
	    break; // do not look over derived table
	}
      if (!tmp)
	return -1;
      else if (!refer)
	return 1;
      else if (tmp == not_found_field && refer == (Item **)not_found_item)
      {
	// call to return error code
	find_field_in_tables(thd, this, tables, 1);
	return -1;
      }
      else if (refer != (Item **)not_found_item)
      {
	Item_ref *r;
	*ref= r= new Item_ref(refer, (char *)table_name,
			   (char *)field_name);
	if (!r)
	  return 1;
	int res;
	if (r->check_cols(1) || r->fix_fields(thd, tables, ref))
	  return 1;
	r->depended_from= last;
	cursel->mark_as_dependent(last);
	thd->add_possible_loop(r);
	return 0;
      }
      else
      {
	depended_from= last;
	/*
	  Mark all selects from resolved to 1 before select where was 
	  found table as depended (of select where was found table)
	*/
	thd->lex.current_select->mark_as_dependent(last);
      }
    } 
    else if (!tmp)
      return -1;

    set_field(tmp);
  }
  else if (thd && thd->set_query_id && field->query_id != thd->query_id)
  {
    /* We only come here in unions */
    TABLE *table=field->table;
    field->query_id=thd->query_id;
    table->used_fields++;
    table->used_keys&=field->part_of_key;
  }
  if (depended_from != 0 && depended_from->having_fix_field)
  {
    *ref= new Item_ref((char *)db_name, (char *)table_name,
		       (char *)field_name);
    if (!*ref)
      return 1;
    return (*ref)->check_cols(1) || (*ref)->fix_fields(thd, tables, ref);
  }
  fixed= 1;
  return 0;
}


void Item::init_make_field(Send_field *tmp_field,
			   enum enum_field_types field_type)
{  
  tmp_field->db_name=(char*) "";
  tmp_field->org_table_name=(char*) "";
  tmp_field->org_col_name=(char*) "";
  tmp_field->table_name=(char*) "";
  tmp_field->col_name=name;
  tmp_field->flags=maybe_null ? 0 : NOT_NULL_FLAG;
  tmp_field->type=field_type;
  tmp_field->length=max_length;
  tmp_field->decimals=decimals;
  if (unsigned_flag)
    tmp_field->flags |= UNSIGNED_FLAG;
}

/* ARGSUSED */
void Item_field::make_field(Send_field *tmp_field)
{
  field->make_field(tmp_field);
  if (name)
    tmp_field->col_name=name;			// Use user supplied name
}

void Item_int::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_LONGLONG);
}

void Item_uint::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_LONGLONG);
  tmp_field->flags|= UNSIGNED_FLAG;
  unsigned_flag=1;
}

void Item_real::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_DOUBLE);
}

void Item_string::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_STRING);
}

void Item_datetime::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_DATETIME);
}


void Item_null::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_NULL);
  tmp_field->length=4;
}


void Item_func::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field, ((result_type() == STRING_RESULT) ?
			      FIELD_TYPE_VAR_STRING :
			      (result_type() == INT_RESULT) ?
			      FIELD_TYPE_LONGLONG : FIELD_TYPE_DOUBLE));
}

void Item_avg_field::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_DOUBLE);
}

void Item_std_field::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_DOUBLE);
}

/*
** Set a field:s value from a item
*/


void Item_field::save_org_in_field(Field *to)
{
  if (field->is_null())
  {
    null_value=1;
    set_field_to_null_with_conversions(to, 1);
  }
  else
  {
    to->set_notnull();
    field_conv(to,field);
    null_value=0;
  }
}

int Item_field::save_in_field(Field *to, bool no_conversions)
{
  if (result_field->is_null())
  {
    null_value=1;
    return set_field_to_null_with_conversions(to, no_conversions);
  }
  else
  {
    to->set_notnull();
    field_conv(to,result_field);
    null_value=0;
  }
  return 0;
}


/*
  Store null in field

  SYNOPSIS
    save_in_field()
    field		Field where we want to store NULL

  DESCRIPTION
    This is used on INSERT.
    Allow NULL to be inserted in timestamp and auto_increment values

  RETURN VALUES
    0	 ok
    1	 Field doesn't support NULL values and can't handle 'field = NULL'
*/   

int Item_null::save_in_field(Field *field, bool no_conversions)
{
  return set_field_to_null_with_conversions(field, no_conversions);
}


/*
  Store null in field

  SYNOPSIS
    save_safe_in_field()
    field		Field where we want to store NULL

  RETURN VALUES
    0	 ok
    1	 Field doesn't support NULL values
*/   

int Item_null::save_safe_in_field(Field *field)
{
  return set_field_to_null(field);
}


int Item::save_in_field(Field *field, bool no_conversions)
{
  int error;
  if (result_type() == STRING_RESULT ||
      result_type() == REAL_RESULT &&
      field->result_type() == STRING_RESULT)
  {
    String *result;
    CHARSET_INFO *cs=charset();
    char buff[MAX_FIELD_WIDTH];		// Alloc buffer for small columns
    str_value.set_quick(buff,sizeof(buff),cs);
    result=val_str(&str_value);
    if (null_value)
      return set_field_to_null_with_conversions(field, no_conversions);
    field->set_notnull();
    error=field->store(result->ptr(),result->length(),cs);
    str_value.set_quick(0, 0, cs);
  }
  else if (result_type() == REAL_RESULT)
  {
    double nr=val();
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    error=field->store(nr);
  }
  else
  {
    longlong nr=val_int();
    if (null_value)
      return set_field_to_null_with_conversions(field, no_conversions);
    field->set_notnull();
    error=field->store(nr);
  }
  return (error) ? -1 : 0;
}


int Item_string::save_in_field(Field *field, bool no_conversions)
{
  String *result;
  result=val_str(&str_value);
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return (field->store(result->ptr(),result->length(),charset())) ? -1 : 0;
}


int Item_int::save_in_field(Field *field, bool no_conversions)
{
  longlong nr=val_int();
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return (field->store(nr)) ? -1 : 0;
}


int Item_real::save_in_field(Field *field, bool no_conversions)
{
  double nr=val();
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return (field->store(nr)) ? -1 : 0;
}

/****************************************************************************
** varbinary item
** In string context this is a binary string
** In number context this is a longlong value.
****************************************************************************/

inline uint char_val(char X)
{
  return (uint) (X >= '0' && X <= '9' ? X-'0' :
		 X >= 'A' && X <= 'Z' ? X-'A'+10 :
		 X-'a'+10);
}

Item_varbinary::Item_varbinary(const char *str, uint str_length)
{
  name=(char*) str-2;				// Lex makes this start with 0x
  max_length=(str_length+1)/2;
  char *ptr=(char*) sql_alloc(max_length+1);
  if (!ptr)
    return;
  str_value.set(ptr,max_length,my_charset_bin);
  char *end=ptr+max_length;
  if (max_length*2 != str_length)
    *ptr++=char_val(*str++);			// Not even, assume 0 prefix
  while (ptr != end)
  {
    *ptr++= (char) (char_val(str[0])*16+char_val(str[1]));
    str+=2;
  }
  *ptr=0;					// Keep purify happy
}

longlong Item_varbinary::val_int()
{
  char *end=(char*) str_value.ptr()+str_value.length(),
       *ptr=end-min(str_value.length(),sizeof(longlong));

  ulonglong value=0;
  for (; ptr != end ; ptr++)
    value=(value << 8)+ (ulonglong) (uchar) *ptr;
  return (longlong) value;
}


int Item_varbinary::save_in_field(Field *field, bool no_conversions)
{
  int error;
  field->set_notnull();
  if (field->result_type() == STRING_RESULT)
  {
    error=field->store(str_value.ptr(),str_value.length(),charset());
  }
  else
  {
    longlong nr=val_int();
    error=field->store(nr);
  }
  return (error) ? -1 :  0;
}


void Item_varbinary::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_STRING);
}

/*
** pack data in buffer for sending
*/

bool Item::send(THD *thd, String *packet)
{
  char buff[MAX_FIELD_WIDTH];
  CONVERT *convert;
  String s(buff,sizeof(buff),packet->charset()),*res;
  if (!(res=val_str(&s)))
    return net_store_null(packet);
  if ((convert=thd->variables.convert_set))
    return convert->store(packet,res->ptr(),res->length());
  return net_store_data(packet,res->ptr(),res->length());
}

bool Item_null::send(THD *thd, String *packet)
{
  return net_store_null(packet);
}

/*
  This is used for HAVING clause
  Find field in select list having the same name
 */

bool Item_ref::fix_fields(THD *thd,TABLE_LIST *tables, Item **reference)
{
  if (!ref)
  {
    SELECT_LEX *sl= thd->lex.current_select->outer_select();
    /*
      Finding only in current select will be performed for selects that have 
      not outer one and for derived tables (which not support using outer 
      fields for now)
    */
    if ((ref= find_item_in_list(this, 
				*(thd->lex.current_select->get_item_list()),
				((sl && 
				  thd->lex.current_select->linkage !=
				  DERIVED_TABLE_TYPE) ? 
				  REPORT_EXCEPT_NOT_FOUND :
				  REPORT_ALL_ERRORS))) ==
	(Item **)not_found_item)
    {
      Field *tmp= (Field*) not_found_field;
      /*
	We can't find table field in table list of current select,
	consequently we have to find it in outer subselect(s).
	We can't join lists of outer & current select, because of scope
	of view rules. For example if both tables (outer & current) have
	field 'field' it is not mistake to refer to this field without
	mention of table name, but if we join tables in one list it will
	cause error ER_NON_UNIQ_ERROR in find_item_in_list.
      */
      SELECT_LEX *last=0;
      for ( ; sl ; sl= sl->outer_select())
      {
	if ((ref= find_item_in_list(this, (last= sl)->item_list,
				   REPORT_EXCEPT_NOT_FOUND)) !=
	   (Item **)not_found_item)
	  break;
	if ((tmp= find_field_in_tables(thd, this,
				       sl->get_table_list(),
				       0)) != not_found_field);
	if (sl->linkage == DERIVED_TABLE_TYPE)
	  break; // do not look over derived table
      }

      if (!ref)
	return 1;
      else if (!tmp)
	return -1;
      else if (ref == (Item **)not_found_item && tmp == not_found_field)
      {
	// Call to report error
	find_item_in_list(this,
			  *(thd->lex.current_select->get_item_list()),
			  REPORT_ALL_ERRORS);
        ref= 0;
	return 1;
      }
      else if (tmp != not_found_field)
      {
	ref= 0; // To prevent "delete *ref;" on ~Item_erf() of this item
	Item_field* f;
	if (!((*reference)= f= new Item_field(tmp)))
	  return 1;
	f->depended_from= last;
	thd->lex.current_select->mark_as_dependent(last);
	return 0;
      }
      else
      {
	depended_from= last;
	thd->lex.current_select->mark_as_dependent(last);
	thd->add_possible_loop(this);
      }
    }
    else if (!ref)
      return 1;
    max_length= (*ref)->max_length;
    maybe_null= (*ref)->maybe_null;
    decimals=	(*ref)->decimals;
  }
  if (((*ref)->with_sum_func && 
       (depended_from || 
	!(thd->lex.current_select->linkage != GLOBAL_OPTIONS_TYPE &&
	  thd->lex.current_select->select_lex()->having_fix_field))) ||
      !(*ref)->fixed)
  {
    my_error(ER_ILLEGAL_REFERENCE, MYF(0), name, 
	     ((*ref)->with_sum_func?
	      "reference on group function":
	      "forward reference in item list"));
    return 1;
  }
  fixed= 1;
  if (ref && (*ref)->check_cols(1))
    return 1;
  return 0;
}

bool Item_ref::check_loop(uint id)
{
  DBUG_ENTER("Item_ref::check_loop");
  if (Item_ident::check_loop(id))
    DBUG_RETURN(1);
  DBUG_RETURN((*ref)->check_loop(id));
}


/*
  If item is a const function, calculate it and return a const item
  The original item is freed if not returned
*/

Item_result item_cmp_type(Item_result a,Item_result b)
{
  if (a == STRING_RESULT && b == STRING_RESULT)
    return STRING_RESULT;
  else if (a == INT_RESULT && b == INT_RESULT)
    return INT_RESULT;
  else if (a == ROW_RESULT || b == ROW_RESULT)
    return ROW_RESULT;
  else
    return REAL_RESULT;
}


Item *resolve_const_item(Item *item,Item *comp_item)
{
  if (item->basic_const_item())
    return item;				// Can't be better
  Item_result res_type=item_cmp_type(comp_item->result_type(),
				     item->result_type());
  char *name=item->name;			// Alloced by sql_alloc

  if (res_type == STRING_RESULT)
  {
    char buff[MAX_FIELD_WIDTH];
    String tmp(buff,sizeof(buff),default_charset_info),*result;
    result=item->val_str(&tmp);
    if (item->null_value)
    {
#ifdef DELETE_ITEMS
      delete item;
#endif
      return new Item_null(name);
    }
    uint length=result->length();
    char *tmp_str=sql_strmake(result->ptr(),length);
#ifdef DELETE_ITEMS
    delete item;
#endif
    return new Item_string(name,tmp_str,length,default_charset_info);
  }
  if (res_type == INT_RESULT)
  {
    longlong result=item->val_int();
    uint length=item->max_length;
    bool null_value=item->null_value;
#ifdef DELETE_ITEMS
    delete item;
#endif
    return (null_value ? (Item*) new Item_null(name) :
	    (Item*) new Item_int(name,result,length));
  }
  else
  {						// It must REAL_RESULT
    double result=item->val();
    uint length=item->max_length,decimals=item->decimals;
    bool null_value=item->null_value;
#ifdef DELETE_ITEMS
    delete item;
#endif
    return (null_value ? (Item*) new Item_null(name) :
	    (Item*) new Item_real(name,result,decimals,length));
  }
}

/*
  Return true if the value stored in the field is equal to the const item
  We need to use this on the range optimizer because in some cases
  we can't store the value in the field without some precision/character loss.
*/

bool field_is_equal_to_item(Field *field,Item *item)
{

  Item_result res_type=item_cmp_type(field->result_type(),
				     item->result_type());
  if (res_type == STRING_RESULT)
  {
    char item_buff[MAX_FIELD_WIDTH];
    char field_buff[MAX_FIELD_WIDTH];
    String item_tmp(item_buff,sizeof(item_buff),default_charset_info),*item_result;
    String field_tmp(field_buff,sizeof(field_buff),default_charset_info);
    item_result=item->val_str(&item_tmp);
    if (item->null_value)
      return 1;					// This must be true
    field->val_str(&field_tmp,&field_tmp);
    return !stringcmp(&field_tmp,item_result);
  }
  if (res_type == INT_RESULT)
    return 1;					// Both where of type int
  double result=item->val();
  if (item->null_value)
    return 1;
  return result == field->val_real();
}


/*****************************************************************************
** Instantiate templates
*****************************************************************************/

#ifdef __GNUC__
template class List<Item>;
template class List_iterator<Item>;
template class List_iterator_fast<Item>;
template class List<List_item>;
#endif
