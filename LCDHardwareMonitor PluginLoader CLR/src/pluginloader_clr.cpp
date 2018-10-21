#pragma unmanaged
#include "LHMAPI.h"
#include "LHMPluginHeader.h"
#include "LHMSensorPlugin.h"
#include "LHMWidgetPlugin.h"

#pragma managed
using namespace System;
using namespace System::Reflection;
using namespace System::Runtime::InteropServices;
[assembly:ComVisible(false)];

[ComVisible(true)]
public interface class
ILHMPluginLoader
{
	b32 LoadSensorPlugin   (void* pluginHeader, void* sensorPlugin);
	b32 UnloadSensorPlugin (void* pluginHeader, void* sensorPlugin);
	b32 LoadWidgetPlugin   (void* pluginHeader, void* widgetPlugin);
	b32 UnloadWidgetPlugin (void* pluginHeader, void* widgetPlugin);
};

public value struct
SensorPlugin_CLR
{
	#define Attributes UnmanagedFunctionPointer(CallingConvention::Cdecl)
	[Attributes] delegate void InitializeDelegate(SP_INITIALIZE_ARGS);
	[Attributes] delegate void UpdateDelegate    (SP_UPDATE_ARGS);
	[Attributes] delegate void TeardownDelegate  (SP_TEARDOWN_ARGS);
	#undef Attributes

	ISensorPlugin^      pluginInstance;
	InitializeDelegate^ initializeDelegate;
	UpdateDelegate^     updateDelegate;
	TeardownDelegate^   teardownDelegate;
};

public value struct
WidgetPlugin_CLR
{
	#define Attributes UnmanagedFunctionPointer(CallingConvention::Cdecl)
	[Attributes] delegate void InitializeDelegate(WP_INITIALIZE_ARGS);
	[Attributes] delegate void UpdateDelegate    (WP_UPDATE_ARGS);
	[Attributes] delegate void TeardownDelegate  (WP_TEARDOWN_ARGS);
	#undef Attributes

	IWidgetPlugin^      pluginInstance;
	InitializeDelegate^ initializeDelegate;
	UpdateDelegate^     updateDelegate;
	TeardownDelegate^   teardownDelegate;
};


public ref struct
LHMPluginLoader : AppDomainManager, ILHMPluginLoader
{
	// NOTE: These functions run in the default AppDomain

	void
	InitializeNewDomain(AppDomainSetup^ appDomainInfo) override
	{
		UNUSED(appDomainInfo);
		InitializationFlags = AppDomainManagerInitializationOptions::RegisterWithHost;
	}

	virtual b32
	LoadSensorPlugin(void* _pluginHeader, void* _sensorPlugin)
	{
		b32 success;
		PluginHeader* pluginHeader = (PluginHeader*) _pluginHeader;
		SensorPlugin* sensorPlugin = (SensorPlugin*) _sensorPlugin;

		success = LoadPlugin(pluginHeader);
		if (!success) return false;

		LHMPluginLoader^ pluginLoader = GetDomainResidentLoader(pluginHeader);
		success = pluginLoader->InitializeSensorPlugin(pluginHeader, sensorPlugin);
		if (!success) return false;

		return true;
	}

	virtual b32
	UnloadSensorPlugin(void* _pluginHeader, void* _sensorPlugin)
	{
		b32 success;
		PluginHeader* pluginHeader = (PluginHeader*) _pluginHeader;
		SensorPlugin* sensorPlugin = (SensorPlugin*) _sensorPlugin;

		LHMPluginLoader^ pluginLoader = GetDomainResidentLoader(pluginHeader);
		success = pluginLoader->TeardownSensorPlugin(pluginHeader, sensorPlugin);
		if (!success) return false;

		success = UnloadPlugin(pluginHeader);
		if (!success) return false;

		return true;
	}

	virtual b32
	LoadWidgetPlugin(void* _pluginHeader, void* _widgetPlugin)
	{
		b32 success;
		PluginHeader* pluginHeader = (PluginHeader*) _pluginHeader;
		WidgetPlugin* widgetPlugin = (WidgetPlugin*) _widgetPlugin;

		success = LoadPlugin(pluginHeader);
		if (!success) return false;

		LHMPluginLoader^ pluginLoader = GetDomainResidentLoader(pluginHeader);
		success = pluginLoader->InitializeWidgetPlugin(pluginHeader, widgetPlugin);
		if (!success) return false;

		return true;
	}

	virtual b32
	UnloadWidgetPlugin(void* _pluginHeader, void* _widgetPlugin)
	{
		b32 success;
		PluginHeader* pluginHeader = (PluginHeader*) _pluginHeader;
		WidgetPlugin* widgetPlugin = (WidgetPlugin*) _widgetPlugin;

		LHMPluginLoader^ pluginLoader = GetDomainResidentLoader(pluginHeader);
		success = pluginLoader->TeardownWidgetPlugin(pluginHeader, widgetPlugin);
		if (!success) return false;

		success = UnloadPlugin(pluginHeader);
		if (!success) return false;

		return true;
	}

private:
	LHMPluginLoader^
	GetDomainResidentLoader (PluginHeader* pluginHeader)
	{
		GCHandle         appDomainHandle = (GCHandle) (IntPtr) pluginHeader->userData;
		AppDomain^       appDomain       = (AppDomain^) appDomainHandle.Target;
		LHMPluginLoader^ pluginLoader    = (LHMPluginLoader^) appDomain->DomainManager;
		return pluginLoader;
	}

	b32
	LoadPlugin(PluginHeader* pluginHeader)
	{
		auto name      = gcnew String(pluginHeader->name);
		auto directory = gcnew String(pluginHeader->directory);

		// NOTE: LHMAppDomainManager is going to get loaded into each new
		// AppDomain so we need to let ApplicationBase get inherited from the
		// default domain in order for it to be found. We set PrivateBinPath so
		// the actual plugin can be found when we load it.
		auto domainSetup = gcnew AppDomainSetup();
		domainSetup->PrivateBinPath     = directory;
		domainSetup->LoaderOptimization = LoaderOptimization::MultiDomainHost;

		// TODO: Shadowcopy and file watch
		//domainSetup.CachePath             = "Cache"
		//domainSetup.ShadowCopyDirectories = true
		//domainSetup.ShadowCopyFiles       = true

		AppDomain^ appDomain = CreateDomain(name, nullptr, domainSetup);
		pluginHeader->userData = (void*) (IntPtr) GCHandle::Alloc(appDomain);
		return true;
	}

	b32
	UnloadPlugin(PluginHeader* pluginHeader)
	{
		GCHandle appDomainHandle = (GCHandle) (IntPtr) pluginHeader->userData;

		AppDomain::Unload((AppDomain^) appDomainHandle.Target);
		appDomainHandle.Free();
		pluginHeader->userData = nullptr;
		return true;
	}



	// NOTE: These functions run in the plugin AppDomain

	SensorPlugin_CLR sensorPluginCLR;
	WidgetPlugin_CLR widgetPluginCLR;

	b32
	InitializeSensorPlugin(PluginHeader* pluginHeader, SensorPlugin* sensorPlugin)
	{
		auto name = gcnew String(pluginHeader->name);

		ISensorPlugin^ pluginInstance = nullptr;
		auto assembly = Assembly::Load(name);
		for each (Type^ type in assembly->GetExportedTypes())
		{
			bool isPlugin = type->GetInterface(ISensorPlugin::typeid->FullName) != nullptr;
			if (isPlugin)
			{
				if (!pluginInstance)
				{
					pluginInstance = (ISensorPlugin^) Activator::CreateInstance(type);
				}
				else
				{
					// TODO: Warning: multiple plugins in same file
				}
			}
		}
		// TODO: Logging
		//LOG_IF(!pluginInstance, "Failed to find a managed sensor plugin class", Severity::Warning, return false);
		sensorPluginCLR.pluginInstance     = pluginInstance;
		sensorPluginCLR.initializeDelegate = gcnew SensorPlugin_CLR::InitializeDelegate(pluginInstance, &ISensorPlugin::Initialize);
		sensorPluginCLR.updateDelegate     = gcnew SensorPlugin_CLR::UpdateDelegate    (pluginInstance, &ISensorPlugin::Update);
		sensorPluginCLR.teardownDelegate   = gcnew SensorPlugin_CLR::TeardownDelegate  (pluginInstance, &ISensorPlugin::Teardown);

		sensorPlugin->initialize = (SensorPluginInitializeFn*) (void*) Marshal::GetFunctionPointerForDelegate(sensorPluginCLR.initializeDelegate);
		sensorPlugin->update     = (SensorPluginUpdateFn*)     (void*) Marshal::GetFunctionPointerForDelegate(sensorPluginCLR.updateDelegate);
		sensorPlugin->teardown   = (SensorPluginTeardownFn*)   (void*) Marshal::GetFunctionPointerForDelegate(sensorPluginCLR.teardownDelegate);

		return true;
	}

	b32
	TeardownSensorPlugin(PluginHeader* pluginHeader, SensorPlugin* sensorPlugin)
	{
		UNUSED(pluginHeader);

		sensorPlugin->initialize = 0;
		sensorPlugin->update     = 0;
		sensorPlugin->teardown   = 0;
		sensorPluginCLR = SensorPlugin_CLR();

		return true;
	}

	b32
	InitializeWidgetPlugin(PluginHeader* pluginHeader, WidgetPlugin* widgetPlugin)
	{
		auto name = gcnew String(pluginHeader->name);

		IWidgetPlugin^ pluginInstance = nullptr;
		auto assembly = Assembly::Load(name);
		for each (Type^ type in assembly->GetExportedTypes())
		{
			bool isPlugin = type->GetInterface(IWidgetPlugin::typeid->FullName) != nullptr;
			if (isPlugin)
			{
				if (!pluginInstance)
				{
					pluginInstance = (IWidgetPlugin^) Activator::CreateInstance(type);
				}
				else
				{
					// TODO: Warning: multiple plugins in same file
				}
			}
		}
		// TODO: Logging
		//LOG_IF(!pluginInstance, "Failed to find a managed sensor plugin class", Severity::Warning, return false);
		widgetPluginCLR.pluginInstance     = pluginInstance;
		widgetPluginCLR.initializeDelegate = gcnew WidgetPlugin_CLR::InitializeDelegate(pluginInstance, &IWidgetPlugin::Initialize);
		widgetPluginCLR.updateDelegate     = gcnew WidgetPlugin_CLR::UpdateDelegate    (pluginInstance, &IWidgetPlugin::Update);
		widgetPluginCLR.teardownDelegate   = gcnew WidgetPlugin_CLR::TeardownDelegate  (pluginInstance, &IWidgetPlugin::Teardown);

		widgetPlugin->initialize = (WidgetPluginInitializeFn*) (void*) Marshal::GetFunctionPointerForDelegate(widgetPluginCLR.initializeDelegate);
		widgetPlugin->update     = (WidgetPluginUpdateFn*)     (void*) Marshal::GetFunctionPointerForDelegate(widgetPluginCLR.updateDelegate);
		widgetPlugin->teardown   = (WidgetPluginTeardownFn*)   (void*) Marshal::GetFunctionPointerForDelegate(widgetPluginCLR.teardownDelegate);

		return true;
	}

	b32
	TeardownWidgetPlugin(PluginHeader* pluginHeader, WidgetPlugin* widgetPlugin)
	{
		UNUSED(pluginHeader);

		widgetPlugin->initialize = 0;
		widgetPlugin->update     = 0;
		widgetPlugin->teardown   = 0;
		widgetPluginCLR = WidgetPlugin_CLR();

		return true;
	}
};

// NOTE: Cross domain function calls average 200ns with the delegate pattern.
// Try playing with security settings if optimizing this
