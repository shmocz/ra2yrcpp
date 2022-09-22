([.tc.body.result.result.classes[] ] | map({key: .pointerSelf|tostring, value: .name}) | from_entries) as $tcmap |
	([.state.body.result.result.state.houses[]] | map({key: .self|tostring, value: .name}) | from_entries) as $housemap |
	[.state.body.result.result.state.objects[]] | map(. + {name: $tcmap[.pointerTechnotypeclass|tostring], player: $housemap[.pointerHouse|tostring],
	stage: .state.body.result.result.state.stage})
