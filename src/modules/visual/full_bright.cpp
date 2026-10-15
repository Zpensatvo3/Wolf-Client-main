#include "full_bright.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../ui_redirect.hpp"
namespace
{
bool read_gamma(const maps::SimpleOption& option, double& output)
	{
		maps::Object value = option ? option.getValue.call() : maps::Object{};
		if (!value || platform::jni_context::clear_exception())
			return false;
		maps::Double number(value.object_instance, false);
		output = number.doubleValue.call();
		return !platform::jni_context::clear_exception();
	}

	bool write_gamma(maps::SimpleOption& option, double value)
	{
		JNIEnv* env = jni::get_env();
		if (!env || !option)
			return false;
		jclass double_class = env->FindClass("java/lang/Double");
		if (!double_class || platform::jni_context::clear_exception())
			return false;
		jmethodID value_of = env->GetStaticMethodID(
			double_class, "valueOf", "(D)Ljava/lang/Double;");
		if (!value_of || platform::jni_context::clear_exception())
			return false;
		jobject boxed = env->CallStaticObjectMethod(double_class, value_of, static_cast<jdouble>(value));
		if (!boxed || platform::jni_context::clear_exception())
			return false;
		// Directly update the stored value so Full Bright is not clamped back to
		// the normal 0.0-1.0 video-settings range by SimpleOption's validator.
		option.value.set(maps::Object(boxed, false));
		return !platform::jni_context::clear_exception();
	}
}

void modules::full_bright::run(::cache& cache)
{
	if (!cache.instance || !jni::get_env())
		return;

	try
	{
		jni::frame frame{ 24 };
		maps::GameOptions options = cache.instance.options.get();
		maps::SimpleOption gamma = options ? options.getGamma.call() : maps::SimpleOption{};
		if (!options || !gamma || platform::jni_context::clear_exception())
			return;
		if (!original_gamma_captured)
			original_gamma_captured = read_gamma(gamma, original_gamma);
		write_gamma(gamma, 16.0);
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}
}

void modules::full_bright::on_disable(::cache& cache)
{
	if (!original_gamma_captured || !cache.instance || !jni::get_env())
	{
		original_gamma_captured = false;
		return;
	}

	try
	{
		jni::frame frame{ 24 };
		maps::GameOptions options = cache.instance.options.get();
		maps::SimpleOption gamma = options ? options.getGamma.call() : maps::SimpleOption{};
		if (gamma && !platform::jni_context::clear_exception())
			write_gamma(gamma, original_gamma);
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}
	original_gamma_captured = false;
}
