#include <math.h>
#include <complex.h>

#include "cvi/dvec/dvec.h"
#include "mml/expr.h"
#include "mml/eval.h"
#include "mml/config.h"
#include "c-hashmap/map.h"

static _Complex double custom_clog2(_Complex double a)
{
	return clog(a)/clog(2.0);
}

static _Complex double custom_clog10(_Complex double a)
{
	return clog(a)/clog(10.0);
}

static _Complex double custom_sqrt(double a)
{
	return csqrt(a + 0.0*I);
}

static MML_Value custom_root(MML_state *state, MML_ExprVec *args)
{
	if (dv_n(*args) == 1)
	{
		return MML_apply_binary_op(state,
				MML_eval_expr(state, dv_a(*args, 0)),
				VAL_INVAL,
				MML_OP_ROOT);
	}

	if (dv_n(*args) != 2)
	{
		MML_log_err("`root`: takes either 2 number (number, complex number, or Boolean) arguments or 1 number argument\n");
		return VAL_INVAL;
	}

	return MML_apply_binary_op(state,
			MML_eval_expr(state, dv_a(*args, 0)),
			MML_eval_expr(state, dv_a(*args, 1)), MML_OP_ROOT);
}

static MML_Value custom_logb(MML_state *state, MML_ExprVec *args)
{
	if (dv_n(*args) == 1)
	{
		MML_Value a = MML_eval_expr(state, dv_a(*args, 0));
		if (!VAL_IS_NUM(a))
		{
			MML_log_err("`logb`: takes either 2 number (real number, complex number, or Boolean) arguments or 1 number argument\n");
			return VAL_INVAL;
		}
		if (a.type != ComplexNumber_type)
			return VAL_NUM(log(MML_get_number(&a)));
		else
			return VAL_CNUM(clog(MML_get_complex(&a)));
	}

	if (dv_n(*args) != 2)
	{
		MML_log_err("`logb`: takes either 2 number (real number, complex number, or Boolean) arguments or 1 number argument\n");
		return VAL_INVAL;
	}

	MML_Value a = MML_eval_expr(state, dv_a(*args, 0));
	MML_Value b = MML_eval_expr(state, dv_a(*args, 1));

	if (!VAL_IS_NUM(a) || !VAL_IS_NUM(b))
	{
		MML_log_err("`logb`: takes either 2 number (real number, complex number, or Boolean) arguments or 1 number argument\n");
		return VAL_INVAL;
	}

	if (a.type != ComplexNumber_type && b.type != ComplexNumber_type)
		return VAL_NUM(log(MML_get_number(&a)) / log(MML_get_number(&b)));
	else
		return VAL_CNUM(clog(MML_get_complex(&a)) / clog(MML_get_complex(&b)));
}

static MML_Value custom_atan2(MML_state *state, MML_ExprVec *args)
{
	if (dv_n(*args) != 2)
	{
		MML_log_err("`atan2`: takes exactly 2 real number arguments\n");
		return VAL_INVAL;
	}

	const MML_Value y = MML_eval_expr(state, dv_a(*args, 0));
	const MML_Value x = MML_eval_expr(state, dv_a(*args, 1));
	if (y.type != RealNumber_type || x.type != RealNumber_type)
	{
		MML_log_err("`atan2` takes exactly 2 real number arguments\n");
		return VAL_INVAL;
	}
	return VAL_NUM(atan2(y.n, x.n));
}

static MML_Value custom_max(MML_state *state, MML_ExprVec *args)
{
	MML_Value max = VAL_INVAL;

	for (size_t i = 0; i < dv_n(*args); ++i)
	{
		if (max.type == Invalid_type)
			max = MML_eval_expr(state, dv_a(*args, i));
		if (!VALTYPE_IS_ORDERED(max))
			return VAL_INVAL;
		MML_Value cur = MML_eval_expr(state, dv_a(*args, i));
		MML_Value tmp = MML_apply_binary_op(state, cur, max, MML_OP_GREATER_TOK);
		if (tmp.type == Boolean_type && tmp.b)
			max = cur;
	}

	return max;
}

static MML_Value custom_min(MML_state *state, MML_ExprVec *args)
{
	MML_Value min = VAL_INVAL;

	for (size_t i = 0; i < dv_n(*args); ++i)
	{
		if (min.type == Invalid_type)
			min = MML_eval_expr(state, dv_a(*args, i));
		if (!VALTYPE_IS_ORDERED(min))
			return VAL_INVAL;
		MML_Value cur = MML_eval_expr(state, dv_a(*args, i));
		MML_Value tmp = MML_apply_binary_op(state, cur, min, MML_OP_LESS_TOK);
		if (tmp.type == Boolean_type && tmp.b)
			min = cur;
	}

	return min;
}

// set this before using compare_values()
static MML_state *cur_state;

static int compare_values(const void *a, const void *b)
{
	const MML_Value va = MML_eval_expr(cur_state, *(MML_Expr **)a);
	const MML_Value vb = MML_eval_expr(cur_state, *(MML_Expr **)b);

	if (!VALTYPE_IS_ORDERED(va) || !VALTYPE_IS_ORDERED(vb))
		return INT32_MIN;
	return MML_get_number(&va) - MML_get_number(&vb);
}

static MML_Value custom_sort(MML_state *state, MML_ExprVec *args)
{
	const MML_ExprVec *vec = &dv_a(*args, 0)->v;
	MML_Expr *ret = calloc(1, sizeof(MML_Expr));
	ret->type = Vector_type;
	ret->should_free_vec_block = true;
	dv_init(ret->v);
	
	dv_copy_to(ret->v, 0, _dv_ptr(*vec), dv_n(*vec));

	dv_push(state->allocd_vecs, ret);

	cur_state = state;
	qsort(ret->v.items, dv_n(ret->v), _dv_item_size(ret->v), compare_values);

	return (MML_Value) { Vector_type, .v = ret->v };
}


static void register_functions(hashmap *maps[6])
{
	hashmap_set(maps[1], hashmap_str_lit("max"),			(uintptr_t)custom_max);
	hashmap_set(maps[1], hashmap_str_lit("min"),			(uintptr_t)custom_min);
	hashmap_set(maps[1], hashmap_str_lit("root"),			(uintptr_t)custom_root);
	hashmap_set(maps[1], hashmap_str_lit("logb"),			(uintptr_t)custom_logb);
	hashmap_set(maps[1], hashmap_str_lit("atan2"),			(uintptr_t)custom_atan2);
	hashmap_set(maps[1], hashmap_str_lit("sort"),			(uintptr_t)custom_sort);

	hashmap_set(maps[2], hashmap_str_lit("sin"),			(uintptr_t)sin);
	hashmap_set(maps[2], hashmap_str_lit("cos"),			(uintptr_t)cos);
	hashmap_set(maps[2], hashmap_str_lit("tan"),			(uintptr_t)tan);
	hashmap_set(maps[2], hashmap_str_lit("asin"),			(uintptr_t)asin);
	hashmap_set(maps[2], hashmap_str_lit("acos"),			(uintptr_t)acos);
	hashmap_set(maps[2], hashmap_str_lit("atan"),			(uintptr_t)atan);
	hashmap_set(maps[2], hashmap_str_lit("sinh"),			(uintptr_t)sinh);
	hashmap_set(maps[2], hashmap_str_lit("cosh"),			(uintptr_t)cosh);
	hashmap_set(maps[2], hashmap_str_lit("tanh"),			(uintptr_t)tanh);
	hashmap_set(maps[2], hashmap_str_lit("asinh"),			(uintptr_t)asinh);
	hashmap_set(maps[2], hashmap_str_lit("acosh"),			(uintptr_t)acosh);
	hashmap_set(maps[2], hashmap_str_lit("atanh"),			(uintptr_t)atanh);
	hashmap_set(maps[2], hashmap_str_lit("ln"),			(uintptr_t)log);
	hashmap_set(maps[2], hashmap_str_lit("log"),			(uintptr_t)log);
	hashmap_set(maps[2], hashmap_str_lit("log2"),			(uintptr_t)log2);
	hashmap_set(maps[2], hashmap_str_lit("log10"),			(uintptr_t)log10);
	hashmap_set(maps[2], hashmap_str_lit("sqrt"),			(uintptr_t)sqrt);
	hashmap_set(maps[2], hashmap_str_lit("floor"),			(uintptr_t)floor);
	hashmap_set(maps[2], hashmap_str_lit("ceil"),			(uintptr_t)ceil);
	hashmap_set(maps[2], hashmap_str_lit("round"),			(uintptr_t)round);

	hashmap_set(maps[3], hashmap_str_lit("complex_sin"),		(uintptr_t)csin);
	hashmap_set(maps[3], hashmap_str_lit("complex_cos"),		(uintptr_t)ccos);
	hashmap_set(maps[3], hashmap_str_lit("complex_tan"),		(uintptr_t)ctan);
	hashmap_set(maps[3], hashmap_str_lit("complex_asin"),		(uintptr_t)casin);
	hashmap_set(maps[3], hashmap_str_lit("complex_acos"),		(uintptr_t)cacos);
	hashmap_set(maps[3], hashmap_str_lit("complex_atan"),		(uintptr_t)catan);
	hashmap_set(maps[3], hashmap_str_lit("complex_sinh"),		(uintptr_t)csinh);
	hashmap_set(maps[3], hashmap_str_lit("complex_cosh"),		(uintptr_t)ccosh);
	hashmap_set(maps[3], hashmap_str_lit("complex_tanh"),		(uintptr_t)ctanh);
	hashmap_set(maps[3], hashmap_str_lit("complex_asinh"),	(uintptr_t)casinh);
	hashmap_set(maps[3], hashmap_str_lit("complex_acosh"),	(uintptr_t)cacosh);
	hashmap_set(maps[3], hashmap_str_lit("complex_atanh"),	(uintptr_t)catanh);
	hashmap_set(maps[3], hashmap_str_lit("complex_ln"),		(uintptr_t)clog);
	hashmap_set(maps[3], hashmap_str_lit("complex_log"),		(uintptr_t)clog);
	hashmap_set(maps[3], hashmap_str_lit("complex_log2"),		(uintptr_t)custom_clog2);
	hashmap_set(maps[3], hashmap_str_lit("complex_log10"),	(uintptr_t)custom_clog10);
	hashmap_set(maps[3], hashmap_str_lit("complex_sqrt"),		(uintptr_t)csqrt);
	hashmap_set(maps[3], hashmap_str_lit("complex_csqrt"),	(uintptr_t)csqrt);

	hashmap_set(maps[4], hashmap_str_lit("csqrt"),			(uintptr_t)custom_sqrt);

	hashmap_set(maps[5], hashmap_str_lit("complex_conj"),		(uintptr_t)conj);
	hashmap_set(maps[5], hashmap_str_lit("complex_phase"),	(uintptr_t)carg);
	hashmap_set(maps[5], hashmap_str_lit("complex_real"),		(uintptr_t)creal);
	hashmap_set(maps[5], hashmap_str_lit("complex_imag"),		(uintptr_t)cimag);


	static constexpr MML_Value TRUE_M		= VAL_BOOL(true);
	static constexpr MML_Value FALSE_M		= VAL_BOOL(false);
	static constexpr MML_Value PI_M		= VAL_NUM(3.14159265358979323846);
	static constexpr MML_Value E_M		= VAL_NUM(2.71828182845904523536);
	static constexpr MML_Value PHI_M		= VAL_NUM(1.61803398874989484820);
	static constexpr MML_Value I_M		= VAL_CNUM(I);
	static constexpr MML_Value NAN_M		= VAL_NUM(NAN);
	static constexpr MML_Value INFINITY_M	= VAL_NUM(INFINITY);

	hashmap_set(maps[0], hashmap_str_lit("true"), 	(uintptr_t)&TRUE_M);
	hashmap_set(maps[0], hashmap_str_lit("false"), 	(uintptr_t)&FALSE_M);
	hashmap_set(maps[0], hashmap_str_lit("pi"),	(uintptr_t)&PI_M);
	hashmap_set(maps[0], hashmap_str_lit("e"),	(uintptr_t)&E_M);
	hashmap_set(maps[0], hashmap_str_lit("phi"),	(uintptr_t)&PHI_M);
	static_assert(I_M.cn == I, "how on earth does I != I? i think your computer's borked\n");
	hashmap_set(maps[0], hashmap_str_lit("i"),	(uintptr_t)&I_M);
	hashmap_set(maps[0], hashmap_str_lit("nan"),	(uintptr_t)&NAN_M);
	hashmap_set(maps[0], hashmap_str_lit("inf"),	(uintptr_t)&INFINITY_M);
}

void math__register_functions(hashmap *maps[6])
{
	register_functions(maps);
}
