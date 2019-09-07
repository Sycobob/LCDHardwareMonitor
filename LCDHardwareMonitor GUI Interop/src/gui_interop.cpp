// TODO: Spend up to 8 ms receiving messages
// TODO: What happens when the simulation tries to send a message larger than the pipe buffer size?
// TODO: Add loops to connecting, reading, and writing (predicated on making progress each iteration)
// TODO: Be careful to ensure the gui and sim never end up doing a blocking write simultaneously
// TODO: Decide whether to support multiple instances or enforce a single instance

#pragma unmanaged
#include "LHMAPI.h"

#include "platform.h"
#include "plugin_shared.h"
#include "gui_protocol.hpp"

#include "platform_win32.hpp"
#include "renderer_d3d9.hpp"

struct State
{
	ConnectionState     simConnection;
	IDirect3D9Ex*       d3d9;
	IDirect3DDevice9Ex* d3d9Device;
	IDirect3DTexture9*  d3d9RenderTexture;
	IDirect3DSurface9*  d3d9RenderSurface0;
};
static State state = {};

#pragma managed
namespace LCDHardwareMonitor::GUI
{
	using namespace System;
	using namespace System::Collections::ObjectModel;
	using namespace System::ComponentModel;
	using namespace System::Diagnostics;
	using mString = System::String;

	public enum struct PluginKind
	{
		Null,
		Sensor,
		Widget,
	};

	public enum struct PluginLoadState
	{
		Null,
		Loaded,
		Unloaded,
		Broken,
	};

	// TODO: Can we use native primitives?
	public value struct PluginInfo
	{
		property UInt32     Ref;
		property mString^   Name;
		property PluginKind Kind;
		property mString^   Author;
		property UInt32     Version;

		property PluginLoadState LoadState; // TODO: Probably belongs in another struct
	};

	public value struct Sensor
	{
		property UInt32   PluginRef;
		property UInt32   Ref;
		property mString^ Name;
		property mString^ Identifier;
		property mString^ Format;
		property Single   Value;
	};

	public value struct WidgetDesc
	{
		property UInt32   PluginRef;
		property UInt32   Ref;
		property mString^ Name;
	};

	public enum struct ProcessState
	{
		Null,
		Launching,
		Launched,
		Terminating,
		Terminated,
	};

	public enum struct MessageType
	{
		Null,
		LaunchSim,
		TerminateSim,
		ForceTerminateSim,
		SetPluginLoadState,
	};

	public value struct Message
	{
		Message(MessageType _type) { type = _type; data = nullptr; }
		Message(MessageType _type, Object^ _data) { type = _type; data = _data; }
		static operator Message (MessageType type) { return Message(type);  }

		MessageType     type;
		Object^ data;
	};

	public ref class SimulationState : INotifyPropertyChanged
	{
	public:
		property UInt32                            Version;
		property IntPtr                            RenderSurface;
		property ObservableCollection<PluginInfo>^ Plugins;
		property ObservableCollection<Sensor>^     Sensors;
		property ObservableCollection<WidgetDesc>^ WidgetDescs;

		// UI Helpers
		property bool         IsSimulationConnected;
		property ProcessState ProcessState;
		property Stopwatch^   ProcessStateTimer;

		// Messages
		System::Collections::Generic::List<Message>^ Messages;

		// Cruft
		SimulationState()
		{
			Plugins           = gcnew ObservableCollection<PluginInfo>();
			Sensors           = gcnew ObservableCollection<Sensor>();
			WidgetDescs       = gcnew ObservableCollection<WidgetDesc>();
			Messages          = gcnew System::Collections::Generic::List<Message>();
			ProcessStateTimer = gcnew Stopwatch();
		}

		virtual event PropertyChangedEventHandler^ PropertyChanged;
		void NotifyPropertyChanged(mString^ propertyName)
		{
			// TODO: Is this safe without the null check?
			PropertyChanged(this, gcnew PropertyChangedEventArgs(propertyName));
		}
	};

	public value struct SetPluginLoadStates
	{
		PluginKind      kind;
		UInt32          ref_;
		PluginLoadState loadState;
	};

	mString^
	ToManagedString(StringSlice cstring)
	{
		LOG_IF((i32) cstring.length < 0, IGNORE,
			Severity::Warning, "Native string truncated");

		// TODO: Remove this
		u32 length = cstring.length;
		while (length > 0 && cstring[length - 1] == '\0')
			length--;

		mString^ result = gcnew mString(cstring.data, 0, (i32) length);
		return result;
	}

	public value struct Interop abstract sealed
	{
		static bool
		Initialize(IntPtr hwnd)
		{
			// DEBUG: Needed for the Watch window
			State& s = state;

			using namespace Message;

			PipeResult result = Platform_CreatePipeClient("LCDHardwareMonitor GUI Pipe", s.simConnection.pipe);
			LOG_IF(result == PipeResult::UnexpectedFailure, return false,
				Severity::Error, "Failed to create pipe for sim communication");

			b32 success = D3D9_Initialize((HWND) (void*) hwnd, s.d3d9, s.d3d9Device);
			if (!success) return false;

			return true;
		}

		static void
		OnDisconnect(SimulationState% simState)
		{
			// DEBUG: Needed for the Watch window
			State& s = state;

			D3D9_DestroySharedSurface(s.d3d9RenderTexture, s.d3d9RenderSurface0);
			simState.RenderSurface = IntPtr::Zero;
			simState.Plugins->Clear();
			simState.Sensors->Clear();
			simState.WidgetDescs->Clear();
			simState.IsSimulationConnected = false;
			simState.NotifyPropertyChanged("");

			ConnectionState& simCon = s.simConnection;
			simCon.sendIndex = 0;
			simCon.recvIndex = 0;
			// TODO: Will need to handle 'pending' states.
			simState.Messages->Clear();
		}

		static bool
		Update(SimulationState^ _simState)
		{
			// DEBUG: Needed for the Watch window
			State& s = state;
			SimulationState% simState = *_simState;

			ConnectionState& simCon = s.simConnection;
			Assert(!simCon.failure);

			while (true)
			{
				Bytes bytes = {};
				defer { List_Free(bytes); };

				// Connection handling
				{
					b32 wasConnected = simCon.pipe.state == PipeState::Connected;
					PipeResult result = Platform_UpdatePipeConnection(simCon.pipe);
					HandleMessageResult(simCon, result);

					b32 isConnected = simCon.pipe.state == PipeState::Connected;
					if (!isConnected && wasConnected) OnDisconnect(simState);
				}
				if (simCon.pipe.state != PipeState::Connected) break;

				// Receive
				i64 startTicks = Platform_GetTicks();
				while (MessageTimeLeft(startTicks))
				{
					b32 success = ReceiveMessage(simCon, bytes);
					if (!success) break;

					using namespace Message;
					Header& header = (Header&) bytes[0];
					switch (header.id)
					{
						default: Assert(false); break;

						case IdOf<Connect>:
						{
							DeserializeMessage<Connect>(bytes);

							Connect& connect = (Connect&) bytes[0];
							simState.Version = connect.version;

							success = D3D9_CreateSharedSurface(
								*s.d3d9Device,
								s.d3d9RenderTexture,
								s.d3d9RenderSurface0,
								(HANDLE) connect.renderSurface,
								connect.renderSize
							);
							if (!success) return false;

							simState.RenderSurface = (IntPtr) s.d3d9RenderSurface0;
							simState.IsSimulationConnected = true;
							simState.NotifyPropertyChanged("");
							break;
						}

						case IdOf<Disconnect>:
						{
							OnDisconnect(simState);
							break;
						}

						case IdOf<PluginsAdded>:
						{
							DeserializeMessage<PluginsAdded>(bytes);
							PluginsAdded& pluginsAdded = (PluginsAdded&) bytes[0];

							for (u32 i = 0; i < pluginsAdded.infos.length; i++)
							{
								PluginInfo mPluginInfo = {};
								mPluginInfo.Ref     = pluginsAdded.refs[i].index;
								mPluginInfo.Name    = ToManagedString(pluginsAdded.infos[i].name);
								mPluginInfo.Kind    = (PluginKind) pluginsAdded.kind;
								mPluginInfo.Author  = ToManagedString(pluginsAdded.infos[i].author);
								mPluginInfo.Version = pluginsAdded.infos[i].version;
								simState.Plugins->Add(mPluginInfo);
							}
							break;
						}

						case IdOf<PluginStatesChanged>:
						{
							DeserializeMessage<PluginStatesChanged>(bytes);
							PluginStatesChanged& statesChanged = (PluginStatesChanged&) bytes[0];

							for (u32 i = 0; i < statesChanged.refs.length; i++)
							{
								for (u32 j = 0; j < (u32) simState.Plugins->Count; j++)
								{
									PluginInfo p = simState.Plugins[(i32) j];
									if ((::PluginKind) p.Kind == statesChanged.kind && p.Ref == statesChanged.refs[i].index)
									{
										p.LoadState = (PluginLoadState) statesChanged.loadStates[i];
										simState.Plugins[(i32) j] = p;
									}
								}
							}
							simState.NotifyPropertyChanged("");
							break;
						}

						case IdOf<SensorsAdded>:
						{
							DeserializeMessage<SensorsAdded>(bytes);

							SensorsAdded& sensorsAdded = (SensorsAdded&) bytes[0];
							for (u32 i = 0; i < sensorsAdded.sensors.length; i++)
							{
								Slice<::Sensor> sensors = sensorsAdded.sensors[i];
								for (u32 j = 0; j < sensors.length; j++)
								{
									::Sensor& sensor = sensors[j];

									Sensor mSensor = {};
									mSensor.PluginRef  = sensorsAdded.pluginRefs[i].index;
									mSensor.Ref        = sensor.ref.index;
									mSensor.Name       = ToManagedString(sensor.name);
									mSensor.Identifier = ToManagedString(sensor.identifier);
									mSensor.Format     = ToManagedString(sensor.format);
									mSensor.Value      = sensor.value;
									simState.Sensors->Add(mSensor);
								}
							}
							break;
						}

						case IdOf<WidgetDescsAdded>:
						{
							DeserializeMessage<WidgetDescsAdded>(bytes);

							WidgetDescsAdded& widgetDescsAdded = (WidgetDescsAdded&) bytes[0];
							for (u32 i = 0; i < widgetDescsAdded.descs.length; i++)
							{
								Slice<::WidgetDesc> widgetDescs = widgetDescsAdded.descs[i];
								for (u32 j = 0; j < widgetDescs.length; j++)
								{
									::WidgetDesc& desc = widgetDescs[j];

									WidgetDesc mWidgetDesc = {};
									mWidgetDesc.PluginRef = widgetDescsAdded.pluginRefs[i].index;
									mWidgetDesc.Ref       = desc.ref.index;
									mWidgetDesc.Name      = ToManagedString(desc.name);
									simState.WidgetDescs->Add(mWidgetDesc);
								}
							}
							break;
						}
					}
				}

				// TODO: Change this to On*() functions called directly from C#, similar to the sim side
				// Construct messages
				for (i32 i = 0; i < simState.Messages->Count; i++)
				{
					using namespace Message;

					Message^ message = simState.Messages[i];
					switch (message->type)
					{
						// TODO: I think I'd rather have the warnings
						default: Assert(false); break;

						case MessageType::LaunchSim:
						case MessageType::ForceTerminateSim:
							// Handled in App
							continue;

						case MessageType::TerminateSim:
						{
							TerminateSimulation nMessage = {};
							b32 success = SerializeAndQueueMessage(simCon, nMessage);
							if (!success) return false;
							break;
						}

						case MessageType::SetPluginLoadState:
							// TODO: Implement
							continue;
					}
				}

				// Send
				while (MessageTimeLeft(startTicks))
				{
					b32 success = SendMessage(simCon);
					if (!success) break;
				}

				break;
			}

			return !simCon.failure;
		}

		static void
		Teardown()
		{
			// DEBUG: Needed for the Watch window
			State& s = state;

			D3D9_DestroySharedSurface(s.d3d9RenderTexture, s.d3d9RenderSurface0);
			D3D9_Teardown(s.d3d9, s.d3d9Device);

			ConnectionState& simCon = s.simConnection;
			Connection_Teardown(simCon);

			s = {};
		}
	};
}
